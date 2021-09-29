//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "CoCCSerumHandler.h"

#include "CoCCUDPTransport.h"
#include "CoCCSerumHeader_m.h"

Define_Module(CoCCSerumHandler);

#define TICKER_MSG_KIND 9024 // randomly chosen


CoCCSerumHandler::CoCCSerumHandler() {

}

CoCCSerumHandler::~CoCCSerumHandler() {

}

void CoCCSerumHandler::initialize() {
    collectionInterval = par("collectionInterval").doubleValue();
    enableRobustCollection = par("enableRobustCollection").boolValue();

    bloomFlowCount = par("bloomFlowCount").intValue();
    bloomErrorRate = par("bloomErrorRate").doubleValue();

    targetUtilization = par("targetUtilization").doubleValue();
    coexistenceMode = static_cast<CoCCUDPTransport::CoexistenceMode>(par("coexistenceMode").intValue());

    enableQueueReduction = par("enableQueueReduction").boolValue();
    acceptableQueueUtilization = par("acceptableQueueUtilization").doubleValue();
    queueReductionTime = SimTime(par("queueReductionTime").doubleValue());

    enableRateControl = par("enableRateControl").boolValue();

    s_pushDelayed = registerSignal("s_pushDelayed");
    s_pushDuplicate = registerSignal("s_pushDuplicate");
    s_mFiltered = registerSignal("s_mFiltered");
    s_bFiltered = registerSignal("s_bFiltered");
    s_mReported = registerSignal("s_mReported");
    s_bReported = registerSignal("s_bReported");
    s_beRate = registerSignal("s_beRate");
    s_qmRate = registerSignal("s_qmRate");
    s_ctrlRate = registerSignal("s_ctrlRate");
    s_qmThresh = registerSignal("s_qmThresh");
    s_numFlows = registerSignal("s_numFlows");

    tickerEvent = new cMessage("CoCC SERUM processing cyclic ticker event", TICKER_MSG_KIND);
    scheduleAt(collectionInterval, tickerEvent);
}

void CoCCSerumHandler::handleMessage(cMessage * const msg) {
    switch (msg->getKind()) {
        case TICKER_MSG_KIND:
            EV_INFO << "CoCC SERUM handler: concluding collection for period " << period << endl;

            concludeCollection();

            scheduleAt(simTime() + collectionInterval, msg);
            break;
        default:
            const int msgKind = msg->getKind();

            delete msg;

            error("Received message with unexpected message kind: %i", msgKind);
            break;
    }
}

void CoCCSerumHandler::concludeCollection() {
    Enter_Method_Silent();

    for (auto iter = ifaces.begin(); iter != ifaces.end(); iter++) {
        auto rr = iter->currentRecord.get();

        if (!mc) {
            error("MonitoringCollector not available");
        }

        // prepare response record

        const MonitoringCollector::Statistics stats = mc->getStatistics(iter->ie->getInterfaceModule());

        // monitoring-based data
        rr->setLineRate(stats.lineRate);
        rr->setAvgUtilization(stats.avgUtilization.filtered());

        // accumulate queue length of all control traffic classes
        double queueLength = 0;
        double queuePktBits = 0;
        long queuePktsCount = 0;
        double queueRate = 0;
        for (auto q : stats.queues) {
            if (!opp_strcmp("queue", q.name)
                    || !opp_strcmp("ctrlEF", q.name)
                    || !opp_strcmp("ctrlPriority", q.name)
                    || !opp_strcmp("ctrlLBE", q.name)) {
                queueLength += q.avgLength.filtered();
                queuePktBits += q.avgPktSize * q.seenPkts;
                queuePktsCount += q.seenPkts;
                queueRate += q.avgIngressRate.filtered();
            }
        }
        rr->setAvgQueueLength(queueLength);

        // CoCC computations

        // determine non-control rate
        double beRate = 0;

        for (auto q : stats.queues) {
            if (!opp_strcmp("BE", q.name)) {
                beRate = std::max(q.avgEgressRate.filtered(), q.avgIngressRate.filtered());
                break;
            }
        }

        // determine maximum target rate considering desired upper bound
        double rate = stats.lineRate * targetUtilization;

        EV_DEBUG << "initial target rate = " << rate << endl;

        if (enableQueueReduction) {
            // compensate for queue length
            const double avgQueuePktBits = queuePktsCount > 0 ? queuePktBits / queuePktsCount : 0;

            rate -= CoCCUDPTransport::coccComputeQueueReduction(queueLength, avgQueuePktBits, acceptableQueueUtilization, queueReductionTime);
        }

        if (enableRateControl) {
            const double expectedRate = CoCCUDPTransport::coccComputeCoexistenceRate(coexistenceMode, rate, iter->accumulator.qmDesiredRate_sum, beRate);

            // compute compensation factor to keep control rate within desired bounds
            const double currentFactor = queueRate > 0 && iter->expectedRateT2 > 0 ? iter->expectedRateT2 / queueRate : 1;
            double controlFactor = iter->rateControlFactor * currentFactor;

            if (controlFactor >= 1) {
                controlFactor = 1;
            } else {
                rate *= controlFactor;

                EV_DEBUG << "rate control activated with queueRate=" << queueRate << " expectedRateT2=" << iter->expectedRateT2 << " currentFactor=" << currentFactor << " old rateControlFactor=" << iter->rateControlFactor << endl;
                EV_DEBUG << "reducing target rate to: " << rate << " with new rateControlFactor=" << controlFactor << endl;
            }

            iter->rateControlFactor = controlFactor;
            // shift unregulated control rates for next round
            iter->expectedRateT2 = iter->expectedRateT1;
            iter->expectedRateT1 = expectedRate; // track expected rate for rate control
        }

        // determine rate available for control
        rate = CoCCUDPTransport::coccComputeCoexistenceRate(coexistenceMode, rate, iter->accumulator.qmDesiredRate_sum, beRate);

        // robust collection mechanism
        if (enableRobustCollection && iter->accumulator.prevCollectedRecords.size() > 0) {
            iter->accumulator.b_sum_filtered += iter->accumulator.prevFallbackRate_sum;

            EV_DEBUG << "robust collection: " << iter->accumulator.prevCollectedRecords.size()
                    << " lost records compensated with prevFallbackRate=" << iter->accumulator.prevFallbackRate_sum << endl;
        }

        // determine link targetQM for concluded interval, this becomes the new threshold to be effective in upcoming interval
        double m_sum = iter->accumulator.m_sum_filtered;
        double b_sum = iter->accumulator.b_sum_filtered;
        double qmThresh = 0; // default to no threshold

        emit(s_mFiltered, m_sum);
        emit(s_bFiltered, b_sum);

        if (iter->accumulator.flows > 0) {
            qmThresh = CoCCUDPTransport::coccComputeLinkTargetQM(rate, m_sum, b_sum, false);

            if (qmThresh < COCC_QM_MIN) {
                // filtered data does not provide enough information to compute qmThresh, e.g. because m = 0 --> use unfiltered collected data
                // this prevents CoCC instances from having no information at all about a link which becomes a bottleneck but was not before

                EV_DEBUG << "qmThresh would become <= 0, returning total sums instead of filtered sums" << endl;
                EV_DEBUG << "filtered sums were: m_sum=" << m_sum << ", b_sum=" << b_sum << endl;

                m_sum = iter->accumulator.m_sum_total;
                b_sum = iter->accumulator.b_sum_total;

                // recompute qmThresh with unfiltered sums since CoCC instances detect and announce their bottleneck based on this information
                qmThresh = CoCCUDPTransport::coccComputeLinkTargetQM(rate, m_sum, b_sum, false);

                EV_DEBUG << "new qmThresh=" << qmThresh << endl;
            }

            qmThresh = CLAMP(qmThresh, COCC_QM_MIN, COCC_QM_MAX); // clamp to valid range before further use
        } else { // no data
            m_sum = 0;
            b_sum = 0;
        }

        EV_DEBUG << "link QM for this round / effective threshold for next round: " << qmThresh << endl;

        emit(s_mReported, m_sum);
        emit(s_bReported, b_sum);
        emit(s_beRate, beRate);
        emit(s_qmRate, iter->accumulator.qmDesiredRate_sum);
        emit(s_qmThresh, qmThresh);
        emit(s_ctrlRate, rate);
        emit(s_numFlows, iter->accumulator.flows);

        // set CoCC response record data
        rr->setPeriod(period);
        rr->setCtrlRate(rate);
        rr->setM_sum(m_sum);
        rr->setB_sum(b_sum);
        rr->setQm_thresh(iter->accumulator.qm_thresh);
        rr->setFlows(iter->accumulator.flows);

        EV_DEBUG << "Prepared CoCC response data for period " << period << " and interface " << iter->ie->getInterfaceModule()->getFullPath() << endl;
        EV_DEBUG << "lineRate=" << rr->getLineRate() << " m_sum=" << rr->getM_sum() << " b_sum=" << rr->getB_sum() << " qmThresh=" << rr->getQm_thresh() << " ctrlRate="<< rr->getCtrlRate() << endl;
        EV_DEBUG << "numFlows=" << rr->getFlows() << " avgQueueLen=" << rr->getAvgQueueLength() << " avgUtil=" << rr->getAvgUtilization() << endl;

        // prepare accumulator for new round
        iter->accumulator.reset();

        iter->accumulator.qm_thresh = qmThresh;
    }

    period++;
}

std::vector<SerumSupport::DatasetInfo> CoCCSerumHandler::datasetDescriptors() {
    const SerumSupport::DatasetInfo pushInfo { DATASET_COCC_PUSH, false }; // tie pushed information to outbound interface
                                                                           // since we are also interested in monitoring information of this interface
    const SerumSupport::DatasetInfo respInfo { -DATASET_COCC_RESP, true }; // initial response record is sent with inverted descriptor
                                                                           // pushed information is tied to inbound interface, hence reverse==true

    return std::vector<SerumSupport::DatasetInfo> { pushInfo, respInfo };
}

void* CoCCSerumHandler::interfaceAdded(const InterfaceEntry * const ie, const short dataDesc) {
    for (auto it = ifaces.begin(); it != ifaces.end(); it++) {
        if (it->interfaceId == ie->getInterfaceId()) {
            // we don't care about the dataDesc, push and resp operate on the same data
            return &(*it);
        }
    }

    ifaces.push_back(CoCCSerumHandler::InterfaceData(this, ie));

    return &ifaces.back();
}

void CoCCSerumHandler::handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    Enter_Method_Silent();

    InterfaceData * const id = static_cast<InterfaceData *>(handlerData);

    ASSERT(id);

    CoCCResponseRecord * const rr = id->currentRecord->dup();

    opts->add(rr);

    // FIXME-SERUM: datagram pkt size is not adjusted

    EV_INFO << "Providing CoCC response data for interface " << id->ie->getInterfaceModule()->getFullPath() << endl;
}

void CoCCSerumHandler::handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    Enter_Method_Silent();

    InterfaceData * const id = static_cast<InterfaceData *>(handlerData);

    ASSERT(id);
    ASSERT(dynamic_cast<CoCCPushRecord *>(&opts->getTlvOption(optIndex)));

    CoCCPushRecord * const pr = static_cast<CoCCPushRecord *>(&opts->getTlvOption(optIndex));

    if (pr->getPeriod() != period) {
        EV_INFO << "in period " << period << ": dropping push for period " << pr->getPeriod() << " from " << pkt->getSrcAddress() << endl;

        emit(s_pushDelayed, true);

        return;
    }

    emit(s_pushDelayed, false);

    const bool added = !id->accumulator.add(pkt->getSrcAddress(), pr);

    emit(s_pushDuplicate, added);

    EV_INFO << "Collected CoCC push data" << endl;
}


CoCCSerumHandler::InterfaceData::InterfaceData(const CoCCSerumHandler * handler, const InterfaceEntry * const ie) :
        ie(ie),
        interfaceId(ie->getInterfaceId()),
        accumulator(CoCCData(handler)) {
    currentRecord.reset(new CoCCResponseRecord());
}

CoCCSerumHandler::CoCCData::CoCCData(const CoCCSerumHandler * handler) :
        curCollectedRecords(BloomFilter(handler->bloomFlowCount, handler->bloomErrorRate)),
        prevCollectedRecords(BloomFilter(handler->bloomFlowCount, handler->bloomErrorRate)) {
}

bool CoCCSerumHandler::CoCCData::add(const IPv6Address &addr, CoCCPushRecord * const &pr) {
    EV_STATICCONTEXT;

    const double m = pr->getM();
    const double b = pr->getB();
    const double qm = pr->getQm_target();
    const double qmDesiredRate = pr->getQmDesiredRate();
    const double lastFallbackRate = pr->getLastFallbackRate();
    CoCCRecordIdentifier id = { addr, pr->getPeriod() };

    EV_DEBUG << "received push with period=" << id.period << " m=" << m << ", b=" << b << ", qm=" << qm
            << " qmDesiredRate=" << qmDesiredRate << " lastFallbackRate=" << lastFallbackRate << " while qmThresh=" << qm_thresh << endl;

    if (curCollectedRecords.contains(id)) {
        EV_DEBUG << "dropping duplicate push for period " << id.period << " from " << addr << endl;

        return false;
    }

    const double qmRate = m * qm + b; // may deviate from the actual sending rate due to linearization errors
                                      // this can cause trouble if this link is not a bottleneck for anyone
                                      // but we can not do much more unless we get the actual fallback rate

    m_sum_total += m;
    b_sum_total += b;

    if (qm >= qm_thresh - 1E-5) {
        m_sum_filtered += m;
        b_sum_filtered += b;
        qmDesiredRate_sum += qmDesiredRate;
    } else {
        b_sum_filtered += qmRate;
        qmDesiredRate_sum += std::min(qmDesiredRate, qmRate);

        EV_DEBUG << "flow is constrained by other link, assuming fixed rate: " << qmRate << endl;
    }

    // bloom-based robust collection mechanism
    // count current static rate estimation into fallback accumulator
    if (qmRate > 1E-5) { // do not count zeroed-out stream stop records
        curFallbackRate_sum += qmRate;
        curCollectedRecords.add(id);
    }
    // subtract previous rate estimation from previous fallback accumulator
    id.period--;
    if (prevCollectedRecords.contains(id)) {
        prevFallbackRate_sum -= lastFallbackRate;
        prevCollectedRecords.remove(id);
    } else {
        EV_WARN << "previous push for period " << id.period << " must have been missed!" << endl;
    }

    flows++;

    return true;
}

void CoCCSerumHandler::CoCCData::reset() {
    m_sum_filtered = m_sum_total = b_sum_filtered = b_sum_total = qm_thresh = qmDesiredRate_sum = 0;
    flows = 0;

    prevFallbackRate_sum = curFallbackRate_sum;
    prevCollectedRecords = curCollectedRecords;

    curFallbackRate_sum = 0;
    curCollectedRecords.reset();
}
