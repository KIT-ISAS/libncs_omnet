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

#include "CoCCUDPTransport.h"
#include "CoCCMsg_m.h"
#include <NcsCpsApp.h>

#include <inet/common/RawPacket.h>
#include <inet/networklayer/diffserv/DSCP_m.h>

Define_Module(CoCCUDPTransport);

#define TC_COCC_EF      DSCP_EF
#define TC_COCC_PRIO    DSCP_CS7
#define TC_COCC_LBE     DSCP_CS1

// randomly chosen ids
#define CONNECT_TICKER_KIND 9810
#define COCC_PUSH_TICKER_MSG_KIND 9811
#define COCC_STREAM_START_EVT_MSG_KIND 9812
#define COCC_STREAM_STOP_EVT_MSG_KIND 9813
#define COCC_COMMIT_EVT_MSG_KIND 9814
#define COCC_EMPTY_PUSH_MSG_KIND 9821

#define COCC_STREAM_START_INTERVALS 3
#define COCC_STREAM_STOP_INTERVALS 0
#define COCC_INACTIVITY_WITHDRAW 5
#define COCC_INACTIVITY_STOP (COCC_INACTIVITY_WITHDRAW + 1)



CoCCUDPTransport::CoCCUDPTransport() {

}

CoCCUDPTransport::~CoCCUDPTransport() {
    for (auto handle : connectionVect) {
        delete handle;
    }

    // no need to delete listenSocket, since it is also stored in connectionVect

    connectionMap.clear();
    connectionVect.clear();
}

CoCCUDPTransport::SocketHandle_t::~SocketHandle_t() {
    if (streamStartEvent && !streamStartEvent->isScheduled()) {
        delete streamStartEvent;
    }
    if (streamStopEvent && !streamStopEvent->isScheduled()) {
        delete streamStopEvent;
    }
    if (pushTicker && !pushTicker->isScheduled()) {
        delete pushTicker;
    }
    if (targetCommitEvent && !targetCommitEvent->isScheduled()) {
        delete targetCommitEvent;
    }
}

void CoCCUDPTransport::initialize() {
    udpIn = gate("udpIn");
    udpOut = gate("udpOut");
    upIn = gate("up$i");
    upOut = gate("up$o");

    s_expectedRate = registerSignal("expectedRate");
    s_rateLimitDropSignal = registerSignal("rateLimitDrop");
    s_reportedM = registerSignal("reportedM");
    s_reportedB = registerSignal("reportedB");
    s_reportedQMtarget = registerSignal("reportedQMtarget");
    s_lastFallbackRate = registerSignal("lastFallbackRate");
    s_qmDesiredRate = registerSignal("qmDesiredRate");
    s_periodMismatch = registerSignal("periodMismatch");
    s_responseMissed = registerSignal("responseMissed");
    s_bottleneckIndex = registerSignal("bottleneckIndex");
    s_bottleneckCtrlRate = registerSignal("bottleneckCtrlRate");
    s_bottleneckShare = registerSignal("bottleneckShare");
    s_bottleneckQM = registerSignal("bottleneckQM");
    s_appliedQM = registerSignal("appliedQM");
    s_pushForced = registerSignal("pushForced");
    s_forcedPushCompensation = registerSignal("forcedPushCompensation");

    collectionInterval = par("collectionInterval").doubleValue();
    enableRobustCollection = par("enableRobustCollection").boolValue();
    enablePushSpreading = par("enablePushSpreading").boolValue();
    autoSpreading = par("autoSpreading").boolValue();
    ackFraction = par("ackFraction").doubleValue();
    maxPushPathDelay = par("maxPushPathDelay").doubleValue();
    regularPushFraction = par("regularPushFraction").doubleValue();
    forcedPushFraction = par("forcedPushFraction").doubleValue();

    if (regularPushFraction < 0 || regularPushFraction > 1) {
        error("regular push fraction violates constraint 0 <= %f <= 1", regularPushFraction);
    }
    if (forcedPushFraction < 0 || forcedPushFraction > (1 - regularPushFraction)) {
        error("forced push fraction violates constraint 0 <= %f <= %f", forcedPushFraction, 1 - regularPushFraction);
    }
    if (maxPushPathDelay < 0 || 2*maxPushPathDelay >= collectionInterval) {
        error("maximum expected path delay violates constraint 0 <= 2x %f < %f", maxPushPathDelay, collectionInterval);
    }

    continousQMAdjustment = par("continousQMAdjustment").boolValue();
    compensateLinearizationErrors = par("compensateLinearizationErrors").boolValue();
    pushCompensationHorizon = std::round(par("pushCompensationHorizon").doubleValue() / collectionInterval.dbl());

    if (pushCompensationHorizon > 0 && pushCompensationHorizon < 2) {
        error("pushCompensationHorizon is unreasonable short, less than 2*collectionInterval");
    }

    enableRateLimiting = par("enableRateLimiting").boolValue();
    lowerLayerOverhead = par("lowerLayerOverhead").intValue();
    metadataOverhead = par("metadataOverhead").intValue();
    permittedBurstSize = par("permittedBurstSize").intValue();

    coexistenceMode = static_cast<CoexistenceMode>(par("coexistenceMode").intValue());
    qmDesired = par("qmDesired").doubleValue();

    if (coexistenceMode < CM_DISABLED || coexistenceMode > CM_TOTAL_SUBMISSION) {
        error("unknown/unsupported coexistenceMode %d", coexistenceMode);
    }
}

void CoCCUDPTransport::handleMessage(cMessage * const msg) {
    if (msg->arrivedOn(udpIn->getId())) {
        switch (msg->getKind()) {
        case UDP_I_DATA: {
            UDPDataIndication * const ctrl = dynamic_cast<UDPDataIndication *>(msg->removeControlInfo());

            ASSERT(ctrl);

            SocketHandle_t * const handle = getSocketById(ctrl->getSockId());

            ASSERT(handle); // incoming UDP packets stay tied to the connected or listen socket

            if (!dynamic_cast<UDPHandshake *>(msg)) {
                SocketHandle_t * const listenHandle = getSocketByAddr(ctrl->getSrcAddr(), ctrl->getSrcPort());

                if (listenHandle != nullptr && listenHandle->connected) {
                    TransportDataInfo * const info = createTransportInfo(ctrl);

                    coccProcessMonitoringRequest(listenHandle, info);
                    coccProcessFeedback(listenHandle, info);

                    msg->setControlInfo(info);

                    if (!dynamic_cast<CoCCMetadataPushPkt *>(msg)) {
                        send(msg, upOut);
                    } else {
                        delete msg; // msg is just an empty push message to forward metadata, can be dropped
                    }
                } else {
                    EV_WARN << "No connection to " << ctrl->getSrcAddr() << " has been established yet. Dropping message: " << msg << endl;

                    delete msg;
                }
            } else {
                UDPHandshake * const hs = dynamic_cast<UDPHandshake *>(msg);

                ASSERT(hs);

                handleIncomingHandshake(hs, ctrl, handle);
            }

            delete ctrl;

            break;
        }
        case UDP_I_ERROR: {
            UDPErrorIndication * const ctrl = dynamic_cast<UDPErrorIndication *>(msg->removeControlInfo());

            ASSERT(ctrl);

            EV_WARN << "UDP error indication: " << ctrl->str() << endl;

            delete ctrl;
            delete msg;

            break;
        }
        default:
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received unexpected message: %s", name);
        }
    } else if (msg->arrivedOn(upIn->getId())) {
        cObject * const ctrlInfo = msg->getControlInfo();

        if (dynamic_cast<TransportConnectReq *>(ctrlInfo)) {
            TransportConnectReq * const req = dynamic_cast<TransportConnectReq *>(ctrlInfo);

            ASSERT(req);

            processConnectRequest(req);

            delete msg;
        } else if (dynamic_cast<TransportListenReq *>(ctrlInfo)) {
            const uint16_t listenPort = dynamic_cast<TransportListenReq *>(ctrlInfo)->getListenPort();

            delete msg;

            processListenRequest(listenPort);
        } else if (dynamic_cast<TransportSetTranslator *>(ctrlInfo)) {
            TransportSetTranslator * const req = dynamic_cast<TransportSetTranslator *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportSetTranslator control info in message.");
            }

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                coccSetTranslator(handle, req->getTranslator(), req->getRole());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportStreamStartInfo *>(ctrlInfo)) {
            TransportStreamStartInfo * const req = dynamic_cast<TransportStreamStartInfo *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportStreamStartInfo control info in message.");
            }

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                coccHandleStreamStart(handle, req->getStart());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportStreamStopInfo *>(ctrlInfo)) {
            TransportStreamStopInfo * const req = dynamic_cast<TransportStreamStopInfo *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportStreamStopInfo control info in message.");
            }

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                coccHandleStreamStop(handle, req->getStop());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportDataInfo *>(ctrlInfo)) {
            TransportDataInfo * const req = dynamic_cast<TransportDataInfo *>(msg->removeControlInfo());

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportDataInfo control info in message.");
            }
            ASSERT(dynamic_cast<RawPacket *>(msg));

            RawPacket * const pkt = dynamic_cast<RawPacket *>(msg);
            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                ASSERT(req->getDstAddr() == handle->remote);

                if (handle->connected) {
                    if (simTime() < handle->stopTime || simTime() > handle->stopTime + collectionInterval) {
                        handle->inactivityCounter = 0;
                    }

                    if (handle->role != NCTXCI_CONTROLLER || !enableRateLimiting || coccRateLimitAccept(handle, pkt)) {
                        inet::UDPSocket::SendOptions opts;

                        opts.networkOptions = req->replaceNetworkOptions();

                        coccReplyToMonitoringRequest(handle, opts.networkOptions); // reply to monitoring request, if pending
                        coccPushMetadata(handle, opts.networkOptions); // initiate push if not already done in period
                        coccCoexistenceHandler(handle, opts.networkOptions); // perform traffic differentiation, if enabled

                        handle->socket->sendTo(pkt, handle->remote, handle->port, &opts);
                    } else {
                        emit(s_rateLimitDropSignal, pkt->getByteLength());

                        EV_INFO << "CoCC rate limiter dropping packet of " << pkt->getByteLength() << " bytes" << endl;

                        delete msg;
                    }
                } else {
                    EV_WARN << "Connection to " << req->getDstAddr() << " is not established yet. Dropping message: " << msg << endl;

                    delete msg;
                }
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;

                delete msg;
            }

            delete req;
        } else {
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received message with unknown control info type: %s", name);
        }
    } else if (msg->isSelfMessage()) {
        // Ticker event

        switch (msg->getKind()) {
        case COCC_PUSH_TICKER_MSG_KIND:
            coccHandlePushTicker(msg);
            break;
        case COCC_STREAM_START_EVT_MSG_KIND:
            coccHandleStreamStart(msg, simTime());
            break;
        case COCC_STREAM_STOP_EVT_MSG_KIND:
            coccHandleStreamStop(msg, simTime());
            break;
        case COCC_COMMIT_EVT_MSG_KIND:
            coccHandleCommitTicker(msg);
            break;
        case CONNECT_TICKER_KIND:
            handleConnectTimeout(msg);
            break;
        default:
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received unexpected message: %s", name);
        }
    } else {
        const char * const name = msg->getName();

        delete msg;

        throw cRuntimeError("Received unexpected message: %s", name);
    }
}

TransportDataInfo * CoCCUDPTransport::createTransportInfo(UDPDataIndication * const ctrl) {
    TransportDataInfo * info = new TransportDataInfo();

    info->setSrcAddr(ctrl->getSrcAddr());
    info->setSrcPort(ctrl->getSrcPort());
    info->setDstAddr(ctrl->getDestAddr());
    info->setDstPort(ctrl->getDestPort());
    info->setNetworkOptions(ctrl->replaceNetworkOptions());

    return info;
}

void CoCCUDPTransport::handleIncomingHandshake(UDPHandshake * const hs,
        UDPDataIndication * const ctrl, SocketHandle_t * const handle) {
    if (!hs->getSynAck()) {
        // listening part, create dedicated handle for sending packets from a listening host
        SocketHandle_t* const txHandle = new SocketHandle_t();

        txHandle->connected = true;
        txHandle->listening = false;
        txHandle->port = ctrl->getSrcPort();
        txHandle->remote = ctrl->getSrcAddr();
        txHandle->socket = handle->socket; // re-use listen-socket

        storeSocket(txHandle);

        // send ACK
        hs->setSynAck(true);
        txHandle->socket->sendTo(hs, txHandle->remote, txHandle->port);
    } else {
        // client side part, set port and flag connection as established
        handle->port = ctrl->getSrcPort();
        handle->connected = true;

        delete hs;

        // confirm connection to upper layer
        auto conf = new TransportConnectReq();
        auto msg = new cMessage("Connection confirmation", CpsConnReq);

        conf->setDstAddr(handle->remote);
        conf->setDstPort(handle->port);
        msg->setControlInfo(conf);

        send(msg, upOut);
    }
}

void CoCCUDPTransport::processConnectRequest(TransportConnectReq* const req) {
    SocketHandle_t* const handle = createSocket();

    connectSocket(handle, req->getDstAddr(), req->getDstPort());
    storeSocket(handle);

    // timeout notification for retries
    cMessage* const selfMsg = new cMessage("Connect Timeout", CONNECT_TICKER_KIND);

    selfMsg->setContextPointer(handle);

    // send connect notification to server and start timeout
    initHandshake(handle, selfMsg);
}

void CoCCUDPTransport::processListenRequest(const uint16_t listenPort) {
    if (listenSocket == nullptr) {
        SocketHandle_t * const handle = createSocket();

        handle->socket->bind(listenPort);
        handle->listening = true;
        handle->port = listenPort;

        listenSocket = handle;

        storeSocket(handle);
    } else {
        throw cRuntimeError("Already listening, refusing to listen twice");
    }
}

void CoCCUDPTransport::handleConnectTimeout(cMessage* const msg) {
    // Timeout-Ticker
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    if (!handle->connected) {
        // retry
        initHandshake(handle, msg);
    } else {
        delete msg;
    }
}

void CoCCUDPTransport::coccProcessMonitoringRequest(SocketHandle_t * const handle, TransportDataInfo * const info) {
    ASSERT(handle);
    ASSERT(info);

    NetworkOptions * const opts = info->getNetworkOptions();

    if (!opts) {
        return;
    }

    const short dstIndex = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_DEST);
    const short hopIndex = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

    if (dstIndex >= 0 && hopIndex >= 0) { // Push and Request are always sent together
        auto doh = dynamic_cast<IPv6DestinationOptionsHeader *>(opts->getV6Header(dstIndex));
        auto hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(hopIndex));

        ASSERT(doh);
        ASSERT(hho);

        if (SerumSupport::containsRequest(doh, DATASET_COCC_RESP)) {
            // store request for response
            handle->pendingRequest.reset(doh->dup());

            // synchronize local push period with push period from message
            ASSERT(SerumSupport::containsPush(hho, DATASET_COCC_PUSH));

            auto *pr = dynamic_cast<CoCCPushRecord *>(SerumSupport::extractPush(hho, DATASET_COCC_PUSH)[0]);

            handle->pushPeriod = pr->getPeriod();

            // schedule response
            coccSchedulePushTicker(handle);
        }
    }

}

double CoCCUDPTransport::coccComputeCoexistenceRate(const CoCCUDPTransport::CoexistenceMode coexistenceMode, const double availableRate,
        const double qmDesiredRate, const double beRate) {
    EV_STATICCONTEXT;

    double result = availableRate;

    EV_DEBUG << "available rate=" << availableRate << "; qmDesiredRate=" << qmDesiredRate << "; beRate=" << beRate << endl;

    if (coexistenceMode > CoCCUDPTransport::CM_DISABLED && availableRate > qmDesiredRate) {
        // available rate is sufficient for qmDesired and coexistence is enabled
        // reduce target rate down to at least the rate required for qmDesired
        result = std::max(qmDesiredRate, availableRate - beRate);

        if (result != availableRate) {
            EV_DEBUG << "coexistence enabled, reducing target rate to " << result << endl;
        }
    }

    return result;
}

double CoCCUDPTransport::coccComputeQueueReduction(const double avgQueueLength, const double avgQueuePktBits,
        const double acceptableQueueUtilization, const simtime_t queueReductionTime) {
    EV_STATICCONTEXT;

    const double excessQueuePkts = avgQueueLength - acceptableQueueUtilization;

    // compensate for standing queue
    if (excessQueuePkts > 0) {
        const double excessBits = excessQueuePkts * avgQueuePktBits;
        const double excessRate = excessBits / queueReductionTime;

        EV_DEBUG << "excess queue [pkts] " << excessQueuePkts << " eqiv. excess data [bit] " << excessBits << " excess rate [bit/s] " << excessRate << endl;

        return excessRate;
    }

    return 0;
}

double CoCCUDPTransport::coccComputeLinkTargetQM(const double targetRate, const double mSum, const double bSum, const bool clamp) {
    double qm;

    if (mSum > 0) {
        qm = (targetRate - bSum) / mSum;
    } else {
        qm = targetRate > bSum ? COCC_QM_MAX + COCC_EPSILON : COCC_QM_MIN - COCC_EPSILON; // no slope known, can only offer binary decision
    }

    return clamp ? CLAMP(qm, COCC_QM_MIN, COCC_QM_MAX): qm;
}

void CoCCUDPTransport::coccProcessFeedback(SocketHandle_t * const handle, TransportDataInfo * const info) {
    ASSERT(handle);
    ASSERT(info);

    NetworkOptions * const opts = info->getNetworkOptions();

    if (handle->role != NCTXCI_CONTROLLER || !opts) {
        return;
    }

    ASSERT(handle->translator);

    const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

    if (index < 0) {
        return;
    }

    auto hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(index));

    ASSERT(hho);

    if (!SerumSupport::containsResponse(hho, DATASET_COCC_RESP)) {
        return;
    }

    // got CoCC feedback, compute new QMTarget

    const std::vector<SerumRecord *> records = SerumSupport::extractResponse(hho, DATASET_COCC_RESP);

    int bottleneckIndex = -1;
    int recordCounter = 0;
    short period = 0;
    bool periodMismatch = false;
    double qmTarget = DBL_MAX; // ensures that one link becomes bottleneck link even if there is no "real" bottleneck
    double qmBottleneck = COCC_QM_MAX;
    double qmBottleneckThresh = COCC_QM_MAX;
    double bottleneckCtrlRate = 0;

    // invariants
    // qmTarget: target QM computed for flow bitrate <= fair share
    //           (according to qmBottleneck and sent linearization)
    //           qmTarget may be higher or lower than qmBottleneck
    // qmBottleneck: bottleneck QM computed according to response record

    const PushData &push = handle->push;

    if (push.sent) {
        EV_DEBUG << "last bottleneckQM=" << push.bottleneckQM << endl;
    }

    for (auto sr : records) {
        CoCCResponseRecord * const r = dynamic_cast<CoCCResponseRecord *>(sr);

        ASSERT(r);

        EV_DEBUG << "CoCC response for link " << records.size() - recordCounter << endl;
        EV_DEBUG << "flows=" << r->getFlows() << " m_sum=" << r->getM_sum() << " b_sum = " << r->getB_sum() << " QM_thresh = " << r->getQm_thresh() << " avgLoad = " << r->getAvgUtilization() << " avgQueue = " << r->getAvgQueueLength() << " ctrlRate = " << r->getCtrlRate() << endl;

        if (r->getPeriod() != push.period && push.sent) {
            periodMismatch = true;

            EV_WARN << "got record from period " << r->getPeriod() << " but expected record from period " << push.period << endl;
        }

        ASSERT(r->getM_sum() > 0 - COCC_EPSILON);

        // compute possible QM for link at current operating point
        double localQM = coccComputeLinkTargetQM(r->getCtrlRate(), r->getM_sum(), r->getB_sum(), false);
        // compute possible QM for cases where the link was previously not a bottleneck
        // in this case, the returned values m,b were computed assuming a constant rate
        // use local knowledge to determine which QM would be possible if this link would be the bottleneck
        double constCompensatedLocalQM = localQM;

        if (push.sent) {
            if (push.bottleneckQM < r->getQm_thresh() - COCC_EPSILON) {
                ICoCCTranslator::CoCCLinearization lin = handle->translator->getLinearizationForRate(handle->translator->getActualQM(), CLAMP(localQM, 0.0, 1.0));
                const double adjustedM = r->getM_sum()                                               + lin.m;
                const double adjustedB = r->getB_sum() - push.bottleneckQM * push.lin.m - push.lin.b + lin.b;

                constCompensatedLocalQM = coccComputeLinkTargetQM(r->getCtrlRate(), adjustedM, adjustedB, false);
            }
        }

        // possible cases
        // 1: localQM == constCompensatedLocalQM
        //      This link is the bottleneck (or one of them), linearization was fully incorporated
        // 2: localQM > constCompensatedLocalQM
        //      This link was no bottleneck previously, const rate was assumed.
        //      Possible QM is lower if this flow would fully request its fair share for the link
        // 3: localQM < constCompensatedLocalQM
        //      This may happen if link assumed all flows to stay constant
        //      (since because it was not a bottleneck for anyone)
        // conclusion:
        //      constCompensatedLocalQM determines theoretical lower QM bound
        //          -> theoretical because it may exceed constant rate QM
        //              -> use constCompensatedLocalQM to locate bottleneck
        //          -> actual bottleneckQM may be not minimum among all links
        //              -> in case 3 links just appear to be the bottleneck with uncompensated calculation
        //      localQM determines bottleneckQM to be used for linearization

        if (localQM < 0 || constCompensatedLocalQM < 0) {
            EV_WARN << "CoCC computation resulted in negative QM_target, link might be congested" << endl;
        }

        EV_DEBUG << "localQM=" << localQM << " constCompensatedLocalQM=" << constCompensatedLocalQM << " qmTarget=" << qmTarget << endl;

        localQM = CLAMP(localQM, COCC_QM_MIN, COCC_QM_MAX);
        // only clamp for negative values, to be able to select most critical link if QM>1 is computed
        constCompensatedLocalQM = std::max(constCompensatedLocalQM, COCC_QM_MIN);

        if (constCompensatedLocalQM < qmTarget) {
            bottleneckIndex = records.size() - recordCounter;

            qmTarget = constCompensatedLocalQM;
            qmBottleneck = localQM;
            qmBottleneckThresh = r->getQm_thresh();
            bottleneckCtrlRate = r->getCtrlRate();
            period = r->getPeriod();
        }

        recordCounter++;
    }

    qmTarget = CLAMP(qmTarget, COCC_QM_MIN, COCC_QM_MAX); // final clamp to ensure qmTarget <= COCC_QM_MAX

    EV_DEBUG << "located bottleneck at link " << bottleneckIndex <<  " with qmTarget=" << qmTarget << " qmBottleneck=" << qmBottleneck << " and qmThresh=" << qmBottleneckThresh << endl;

    // preserve computed bottleneck QM for next period
    handle->feedback.period = period;
    handle->feedback.valid = true;
    handle->feedback.bottleneckQM = qmBottleneck;

    emit(this->s_periodMismatch, periodMismatch);
    emit(this->s_bottleneckIndex, bottleneckIndex);
    emit(this->s_bottleneckCtrlRate, bottleneckCtrlRate);
    emit(s_bottleneckQM, qmBottleneck);

    const double actualQM = handle->translator->getActualQM();

    if (push.sent) {
        // rate which would be required for qmTarget
        const double realRate = handle->translator->getRateForQM(actualQM, qmTarget);
        // possible rate if flow is assumed to have constant rate by (previously) non-bottleneck links
        const double constQmRate = push.lin.m * push.bottleneckQM + push.lin.b;
        // rate which would be expected according to linearization
        const double expectedLinRate = push.lin.m * qmBottleneck + push.lin.b;

        double rateLimit = realRate;

        if (qmTarget != qmBottleneck) {
            // cases 2+3, continue at constant rate until switch to new bottleneck is done
            rateLimit = constQmRate;

            emit(s_bottleneckShare, constQmRate);

            EV_DEBUG << "Applying fixed rate for last bottleneckQM (" << push.bottleneckQM << "): " << constQmRate << "; real rate (for qmTarget):" << realRate << endl;
        } else { // qmTarget == qmBottleneck
            // last linearization was incorporated at bottleneck, compensate for linearization errors or cap at constQmRate
            const double bottleneckShare = std::min(constQmRate, expectedLinRate);

            emit(s_bottleneckShare, bottleneckShare);

            const double rateIncrease = rateLimit / std::max(bottleneckShare, COCC_EPSILON); // expected rate might be negative

            EV_DEBUG << "Expected rate (for targetQM = " << qmTarget << ") " << expectedLinRate << " bit/s; real rate " << realRate << " bit/s; rateIncrease " << rateIncrease << endl;
            EV_DEBUG << "based on m=" << push.lin.m << " and b=" << push.lin.b << " - originally announced rate: " << constQmRate << endl;

            // actual rate for new targetQM is higher then expected, limit to stay within expected rate
            if (compensateLinearizationErrors && rateIncrease > 1 + COCC_EPSILON) {
                rateLimit = bottleneckShare;

                EV_DEBUG << "Capping rate to expected/const bottleneck rate" << endl;
            }
        }

        // compute new qmTarget if rate Limit has been applied
        if (std::abs(rateLimit - realRate) > COCC_EPSILON) {
            const double oldQmValue = qmTarget;

            qmTarget = handle->translator->getQMForRate(oldQmValue, rateLimit);
            const double newRate = handle->translator->getRateForQM(actualQM, qmTarget);

            EV_DEBUG << "Limiting rate to " << rateLimit << ", bottleneck QM = " << oldQmValue << ", QM for rate limit = " << qmTarget << endl;

            if (qmTarget < 0 || newRate > rateLimit + 1) {
                EV_DEBUG << "Unable to further reduce QM to achieve limited rate, link might become congested. New rate: " << handle->translator->getRateForQM(actualQM, 0) << endl;
            }
        }

        qmTarget = CLAMP(qmTarget, COCC_QM_MIN, COCC_QM_MAX);
    } else {
        qmTarget = 0;
    }

    if (qmTarget > COCC_QM_MAX || qmTarget < COCC_QM_MIN) {
        EV_WARN << "Computed unexpected qmTarget value of " << qmTarget << ", clamping" << endl;
    }

    qmTarget = CLAMP(qmTarget, COCC_QM_MIN, COCC_QM_MAX);

    EV_DEBUG << "Next targetQM = " << qmTarget << endl;

    handle->feedback.targetQM = qmTarget;
    handle->feedback.targetBitRate = handle->translator->getRateForQM(actualQM, qmTarget);

    const simtime_t commitTime = (handle->feedback.period + 1 + ackFraction) * collectionInterval;

    if (simTime() < commitTime) {
        scheduleAt(commitTime, handle->targetCommitEvent);

        EV_DEBUG << "commit scheduled for " << commitTime << endl;
    } else {
        EV_WARN << "applying targetQM too late. planned commit time: " << commitTime << " actual commit time: " << simTime() << endl;

        coccHandleCommitTicker(handle->targetCommitEvent);
    }
}

void CoCCUDPTransport::coccHandleStreamStart(cMessage * const msg, simtime_t start) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    coccHandleStreamStart(handle, start);
}

void CoCCUDPTransport::coccHandleStreamStart(SocketHandle_t * const handle, simtime_t start) {
    if (start > simTime() + COCC_STREAM_START_INTERVALS * collectionInterval) {
        if (handle->streamStartEvent) {
            if (handle->streamStartEvent->isScheduled()) {
                cancelEvent(handle->streamStartEvent);
            }
        } else {
           handle->streamStartEvent = new cMessage("CoCC stream start event", COCC_STREAM_START_EVT_MSG_KIND);
           handle->streamStartEvent->setContextPointer(handle);
        }

        // schedule for later start
        scheduleAt(start - COCC_STREAM_START_INTERVALS * collectionInterval, handle->streamStartEvent);
    } else {
        // trigger state reset, as if push had ben stopped due to inactivity
        handle->inactivityCounter = COCC_INACTIVITY_STOP;
        coccHandleDrivingPushTicker(handle);
        // schedule push in upcoming interval
        handle->inactivityCounter = 0;
        handle->stopTime = SIMTIME_ZERO;
        coccSchedulePushTicker(handle);
    }
}

void CoCCUDPTransport::coccHandleStreamStop(cMessage * const msg, simtime_t stop) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    coccHandleStreamStop(handle, stop + collectionInterval * COCC_STREAM_STOP_INTERVALS);
}

void CoCCUDPTransport::coccHandleStreamStop(SocketHandle_t * const handle, simtime_t stop) {
    ASSERT(stop >= collectionInterval);

    const ulong stopPeriod = static_cast<ulong>(stop / collectionInterval) - COCC_STREAM_STOP_INTERVALS;
    handle->stopTime = stopPeriod * collectionInterval;

    if (simTime() < handle->stopTime) {
        if (handle->streamStopEvent) {
            if (handle->streamStopEvent->isScheduled()) {
                cancelEvent(handle->streamStopEvent);
            }
        } else {
           handle->streamStopEvent = new cMessage("CoCC stream stop event", COCC_STREAM_STOP_EVT_MSG_KIND);
           handle->streamStopEvent->setContextPointer(handle);
        }

        // schedule for later start
        scheduleAt(handle->stopTime, handle->streamStopEvent);
    } else {
        EV_DEBUG << "Received stream stop signal, ticker will time out after this period." << endl;

        handle->inactivityCounter = COCC_INACTIVITY_WITHDRAW;
    }
}

void CoCCUDPTransport::coccSetTranslator(SocketHandle_t * const handle, ICoCCTranslator* const translator, const NcsContextComponentIndex role) {
    auto oldRole = handle->role;
    auto oldPtr = handle->translator;

    handle->role = translator != nullptr ? role : NCTXCI_COUNT;
    handle->translator = translator;

    ASSERT(oldRole == handle->role
            || (oldRole == NCTXCI_COUNT && handle->role != NCTXCI_COUNT));

    if (oldPtr == nullptr) {
        // prepare push ticker
        handle->collectionPeriodStart = 0;
        handle->pushTicker = new cMessage("CoCC push ticker event", COCC_PUSH_TICKER_MSG_KIND);
        handle->pushTicker->setContextPointer(handle);
        handle->targetCommitEvent = new cMessage("CoCC target QoC commit ticker event", COCC_COMMIT_EVT_MSG_KIND);
        handle->targetCommitEvent->setContextPointer(handle);
    }

    if (role == NCTXCI_SENSOR || role == NCTXCI_ACTUATOR) {
        // nothing more to do here, the remainder of this function sets up state for the driving end of a connection
        return;
    }

    if (continousQMAdjustment) {
        // register at translator to observe control events and adjust targetQM if required
        translator->setControlObserver(this, handle);
    }

    // set overhead values to allow for more precise calculations in translator
    translator->setPerPacketOverhead(lowerLayerOverhead);
    if (pushCompensationHorizon > 0) {
        // presume initial forced push, thus add a full lowerLayerOverhead
        translator->setNetworkOverhead((metadataOverhead + lowerLayerOverhead) / collectionInterval.dbl());
    } else {
        translator->setNetworkOverhead(metadataOverhead / collectionInterval.dbl());
    }

    // update expected bit rate for rate limiting
    handle->expectedBitRate = translator->getRateForQM(translator->getActualQM(), translator->getTargetQM());

    if (oldPtr != nullptr) {
        oldPtr->setControlObserver(nullptr); // unregister
    }
}

bool CoCCUDPTransport::coccReplyToMonitoringRequest(SocketHandle_t * const handle, NetworkOptions* &opts) {
    ASSERT(handle);

    if (handle->pendingRequest) {
        ASSERT(handle->forcedPushTime > handle->pushStart);

        const bool forced = simTime() == handle->forcedPushTime;

        if (handle->pushStart > simTime()) {
            // too early, reply phase has yet to come
            return false;
        }

        if (!forced && handle->pushWaitCounter > 0) {
            // postphone to a later opportunity
            handle->pushWaitCounter--;

            return false;
        }

        if (!opts) {
            opts = new NetworkOptions();
        }

        opts->setTrafficClass(TC_COCC_EF);

        IPv6HopByHopOptionsHeader * hho = nullptr;
        const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

        if (index >= 0) {
            hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(index));
            ASSERT(hho);
        } else {
            hho = new IPv6HopByHopOptionsHeader();

            opts->addV6Header(hho);
        }

        SerumSupport::initiateResponse(handle->pendingRequest.get(), hho, DATASET_COCC_RESP);
        handle->pendingRequest.reset();

        cancelEvent(handle->pushTicker);

        emit(s_pushForced, forced);

        EV_DEBUG << "initiating response for period " << handle->pushPeriod << endl;
    }

    return true;
}

void CoCCUDPTransport::coccSchedulePushTicker(SocketHandle_t * const handle) {
    ASSERT(handle->translator);

    const bool driving = handle->role == NCTXCI_CONTROLLER;

    // global period reference
    const ulong periodNumber = static_cast<ulong>(simTime() / collectionInterval);

    // determine time of next push period
    if (driving) {
        // driving instances compute period from time
        handle->collectionPeriodStart += collectionInterval;
        handle->pushPeriod++;
        if (handle->collectionPeriodStart + collectionInterval < simTime()) {
            // skipped push events due to inactive application, resync push interval
            handle->collectionPeriodStart = (periodNumber + 1) * collectionInterval;
            handle->pushPeriod = (short) (periodNumber + 1);

            EV_DEBUG << "CCoC push ticker resynchronized" << endl;
        }
    } else {
        // responding instances compute period from last request
        // period already got extracted from PushRecord

        // find actual start of collectionPeriod in global time
        if (handle->pushPeriod == (short) periodNumber) {
            handle->collectionPeriodStart = periodNumber * collectionInterval;
        } else if (handle->pushPeriod == (short) (periodNumber - 1)) {
           // slightly delayed, push/request is from last period
            handle->collectionPeriodStart = (periodNumber - 1) * collectionInterval;
        } else {
            handle->collectionPeriodStart = SIMTIME_ZERO; // drop request
        }
    }

    // compute window sizes and push parameters
    const simtime_t totalPushWindow = collectionInterval - 2 * maxPushPathDelay; // total time available for push
    const simtime_t pushWindow = totalPushWindow * (driving ? 1 - ackFraction : ackFraction); // time available for this push phase
    const simtime_t pushWindowOffset = driving ? ackFraction * totalPushWindow + maxPushPathDelay : collectionInterval; // offset of push window start relative to collection interval

    handle->pushStart = handle->collectionPeriodStart + pushWindowOffset;

    const double qmActual = handle->translator->getActualQM();
    const double qmTarget = handle->translator->getTargetQM();

    const double expectedFrequency = handle->translator->getAvgFrequencyForQM(qmActual, qmTarget);
    const long windowFits = (pushWindow * regularPushFraction * expectedFrequency).dbl();
    const double regularLikelyhood = std::min(1.0, (pushWindow * regularPushFraction * expectedFrequency).dbl());
    const double effectiveForcedPushFraction = autoSpreading ? std::max(forcedPushFraction, 1 - regularLikelyhood) : forcedPushFraction;

    handle->forcedPushTime = handle->collectionPeriodStart + pushWindowOffset + pushWindow * uniform(1 - effectiveForcedPushFraction, 1);
    handle->effectiveForcedPushFraction = effectiveForcedPushFraction;

    if (windowFits <= 1) {
        handle->pushWaitCounter = 0;
    } else {
        // spread push only for responding instances or if pushSpreading is enabled
        handle->pushWaitCounter = !driving || enablePushSpreading ? intuniform(0, windowFits - 1) : 0;
    }


    EV_DEBUG << "scheduling push ticker. period: " << handle->pushPeriod << " start: " << handle->collectionPeriodStart << " push start: " << handle->pushStart << endl;
    EV_DEBUG << "window: " << pushWindow << " offset: " << pushWindowOffset << " forced push at: " << handle->forcedPushTime << " regular at packet: " << handle->pushWaitCounter << endl;

    // reschedule push ticker
    cancelEvent(handle->pushTicker);

    if (handle->forcedPushTime >= simTime()) {
        scheduleAt(handle->forcedPushTime, handle->pushTicker);

        if (!driving) {
            emit(s_responseMissed, false);
            emit(s_periodMismatch, false);
        }
    } else {
        // we are way to late. this should only be possible for responding instances
        ASSERT(!driving);

        // lets see if there is a chance to at least provide delayed feedback
        if (handle->pushStart + collectionInterval - maxPushPathDelay < simTime()) {
            // push will arrive before end of period. immediately force a push
            handle->forcedPushTime = simTime();

            scheduleAt(handle->forcedPushTime, handle->pushTicker);

            emit(s_periodMismatch, false);
        } else {
            handle->pendingRequest.reset();

            emit(s_responseMissed, true);
            emit(s_periodMismatch, true);
        }
    }
}

bool CoCCUDPTransport::coccPushMetadata(SocketHandle_t * const handle, NetworkOptions* &opts) {
    ASSERT(handle);

    if (handle->role == NCTXCI_CONTROLLER && handle->pushStart <= simTime()) {
        ASSERT(handle->translator);

        const double qmActual = handle->translator->getActualQM();
        const double qmTarget = handle->translator->getTargetQM();

        if (handle->pushWaitCounter > 0) {
            handle->pushWaitCounter--;

            return false; // push spreading is in effect, wait until next time
        }

        if (handle->push.sent) {
            emit(s_responseMissed, !handle->feedback.valid || handle->push.period != handle->feedback.period);
        }

        const bool pushForced = handle->inactivityCounter > 0 && simTime() == handle->forcedPushTime;

        if (pushCompensationHorizon > 0) {
            const double likelyhood = coccEstimateForcedPushLikelyhood(handle);

            // keep track of actual push likelyhood and expected push likelyhood
            handle->expectedPushHistory.push(likelyhood);
            handle->forcedPushHistory.push(pushForced ? 1.0 : 0.0);

            double compensationFactor = likelyhood;

            // compute how far off the expected likelyhood is
            if (handle->expectedPushHistory.mean() > 1E-9) {
                compensationFactor *= handle->forcedPushHistory.mean() / handle->expectedPushHistory.mean();
            }

            // avoid overcompensation
            compensationFactor = CLAMP(compensationFactor, 0.0, 1.0);

            // apply compensation
            handle->translator->setNetworkOverhead((metadataOverhead + lowerLayerOverhead * compensationFactor) / collectionInterval.dbl());

            emit(s_forcedPushCompensation, compensationFactor);
        }

        // compute current metadata and add them to push record

        PushData &push = handle->push;
        FeedbackData &feedback = handle->feedback;

        if (!push.sent) { // invalid data
            // reset status e.g. to not announce a fallback rate > 0
            push.lin = { 0, 0 };
            push.bottleneckQM = 0;
        }

        // use computed bottleneck QM if available to avoid signaling a lower target QM due to temporary linearization error compensation
        double qmBottleneckTarget = feedback.valid ? feedback.bottleneckQM : qmTarget;
        ICoCCTranslator::CoCCLinearization lin = handle->translator->getLinearizationForRate(qmActual, qmBottleneckTarget);
        // desired rate is used in routers to handle coexistence
        double qmDesiredRate = handle->translator->getRateForQM(qmActual, qmDesired);
        const double lastFallbackRate = push.lin.m * push.bottleneckQM + push.lin.b;

        if (handle->inactivityCounter >= COCC_INACTIVITY_WITHDRAW) {
            lin = { 0, 0 };
            qmBottleneckTarget = 0;
            qmDesiredRate = 0;
            // must not change last fallback rate to remove allocated compensation rate
        }

        push.sent = true;
        push.period = handle->pushPeriod;
        push.bottleneckQM = qmBottleneckTarget; // remember used value
        push.lin = lin;

        CoCCPushRecord * const pr = new CoCCPushRecord();

        pr->setPeriod(handle->pushPeriod);
        pr->setM(lin.m);
        pr->setB(lin.b);
        pr->setQm_target(qmBottleneckTarget);
        pr->setQmDesiredRate(qmDesiredRate);
        pr->setLastFallbackRate(lastFallbackRate);

        emit(s_reportedM, pr->getM());
        emit(s_reportedB, pr->getB());
        emit(s_reportedQMtarget, pr->getQm_target());
        emit(s_lastFallbackRate, pr->getLastFallbackRate());
        emit(s_qmDesiredRate, pr->getQmDesiredRate());
        emit(s_pushForced, pushForced);

        // add request record to query metadata
        CoCCRequestRecord * const rr = new CoCCRequestRecord();

        if (!opts) {
            opts = new NetworkOptions();
        }

        opts->setTrafficClass(TC_COCC_EF); // coccCoexistenceHandler() will rewrite this if enableRobustCollection==True

        {
            IPv6HopByHopOptionsHeader * hho = nullptr;

            const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

            if (index >= 0) {
                hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(index));
                ASSERT(hho);
            } else {
                hho = new IPv6HopByHopOptionsHeader();

                opts->addV6Header(hho);
            }

            hho->getTlvOptions().add(pr);
        }
        {
            IPv6DestinationOptionsHeader * doh = nullptr;

            const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_DEST);

            if (index >= 0) {
                auto doh = dynamic_cast<IPv6DestinationOptionsHeader *>(opts->getV6Header(index));
                ASSERT(doh);
            } else {
                doh = new IPv6DestinationOptionsHeader();

                opts->addV6Header(doh);
            }

            doh->getTlvOptions().add(rr);
        }

        EV_DEBUG << "CoCC metadata push period=" << pr->getPeriod() << " m=" << pr->getM() << "; b=" << pr->getB()
                << "; bottleneckQMTarget=" << pr->getQm_target() << "; lastFallbackRate=" << pr->getLastFallbackRate()
                << "; with actualQM=" << qmActual << " and qmDesiredRate=" << pr->getQmDesiredRate() << endl;

        coccSchedulePushTicker(handle);

        return true;
    }

    return false;
}

double CoCCUDPTransport::coccEstimateForcedPushLikelyhood(SocketHandle_t * const handle) {
    ASSERT(handle);
    ASSERT(handle->translator);
    ASSERT(handle->role == NCTXCI_CONTROLLER);

    const simtime_t totalPushWindow = collectionInterval - 2 * maxPushPathDelay; // total time available for push
    const simtime_t pushWindow = totalPushWindow * (1 - ackFraction); // time available for this push phase
    const simtime_t regularWindow = pushWindow * (1 - handle->effectiveForcedPushFraction / 2); // effective time available for regular push
    const double expectedFrequency = handle->translator->getAvgFrequencyForQM(handle->translator->getActualQM(), handle->translator->getTargetQM());
    const double windowFits = (regularWindow * expectedFrequency).dbl();

    const double regularLikelyhood = std::min(1.0, windowFits);

    ASSERT(regularLikelyhood >= 0 && regularLikelyhood <= 1.0);

    return 1.0 - regularLikelyhood;
}

void CoCCUDPTransport::coccCoexistenceHandler(SocketHandle_t * const handle, NetworkOptions* &opts) {
    if (handle->role == NCTXCI_CONTROLLER) { // nothing to do for non-controller devices
        ASSERT(handle->translator);

        if (!opts) {
            opts = new NetworkOptions();
        }

        // ACKs and metadata push/request packets are already flagged as EF, do not change
        int tc = opts->getTrafficClass();

        // flag all packets which are not default BE class as high priority
        tc = (tc != TC_COCC_EF) ? TC_COCC_PRIO : tc;

        switch (coexistenceMode) {
        case CM_REACTIVE: {
            // only in reactive mode: divide flows between TC_COCC_PRIO and TC_COCC_LBE
            const double actualQM = handle->translator->getActualQM();
            const double targetQM = handle->translator->getTargetQM();
            const double targetQMRate = handle->translator->getRateForQM(targetQM, targetQM);
            const double desiredQMRate = handle->translator->getRateForQM(actualQM, qmDesired);
            // fraction of targetQMRate which is required to have better QM than qmDesired
            const double lbeFraction = CLAMP((targetQMRate - desiredQMRate) / targetQMRate, 0.0, 1.0);

            handle->lbeClassAccumulator += lbeFraction;

            if (tc != TC_COCC_EF && handle->lbeClassAccumulator >= 1) {
                tc = TC_COCC_LBE;
                handle->lbeClassAccumulator -= 1;
            }
            } break;
        case CM_SUBMISSION: {
            if (tc != TC_COCC_EF) {
                tc = TC_COCC_LBE;
            }
            } break;
        case CM_TOTAL_SUBMISSION: {
            tc = TC_COCC_LBE;
            } break;
        default:
            break;
        }

        if (enableRobustCollection && tc == TC_COCC_EF) {
            tc = TC_COCC_PRIO; // EF forwarding is not required if robust collection is enabled
        }

        opts->setTrafficClass(tc);
    }
}

void CoCCUDPTransport::coccHandlePushTicker(cMessage * const msg) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    const bool driving = handle->role == NCTXCI_CONTROLLER;

    handle->pushWaitCounter = 0; // no more waiting, push will be forced now

    if (driving) {
        coccHandleDrivingPushTicker(handle);
    } else {
        coccHandleRespondingPushTicker(handle);
    }
}

void CoCCUDPTransport::coccHandleDrivingPushTicker(SocketHandle_t * const handle) {
    if (handle->connected && handle->inactivityCounter < COCC_INACTIVITY_STOP ) {
        inet::UDPSocket::SendOptions opts;

        handle->inactivityCounter++;

        // initiate push if not already done in period
        if (coccPushMetadata(handle, opts.networkOptions)) {
            cPacket * const pkt = new CoCCMetadataPushPkt("CoCC metadata push packet", COCC_EMPTY_PUSH_MSG_KIND);

            handle->socket->sendTo(pkt, handle->remote, handle->port, &opts);
        }
    }

    // reschedule ticker timer one interval into future
    if (!handle->pushTicker->isScheduled()) {
        if (handle->inactivityCounter < COCC_INACTIVITY_STOP) {
            // required if connection is not established yet to continue probing
            coccSchedulePushTicker(handle);
        } else {
            handle->collectionPeriodStart = 0;

            handle->push.sent = false;
            handle->feedback.valid = false;

            handle->lbeClassAccumulator = 0;

            EV_DEBUG << "CCoC push ticker disabled due to inactivity" << endl;

            handle->translator->setTargetQM(0);
            handle->expectedBitRate = handle->translator->getRateForQM(handle->translator->getActualQM(), 0);

            EV_DEBUG << "Resetting target QM to 0" << endl;

            cancelEvent(handle->targetCommitEvent);
        }
    }
}

void CoCCUDPTransport::coccHandleRespondingPushTicker(SocketHandle_t * const handle) {
    if (handle->connected) {
        inet::UDPSocket::SendOptions opts;

        coccReplyToMonitoringRequest(handle, opts.networkOptions);

        cPacket * const pkt = new CoCCMetadataPushPkt("CoCC monitoring reply packet", COCC_EMPTY_PUSH_MSG_KIND);

        handle->socket->sendTo(pkt, handle->remote, handle->port, &opts);
    }
}

void CoCCUDPTransport::coccHandleCommitTicker(cMessage * const msg) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    if (handle->translator && handle->feedback.valid) { // translator might be gone, this check is not for controller vs. non-controller
        handle->expectedBitRate = handle->feedback.targetBitRate;

        const double targetQM = handle->translator->getQMForRate(handle->translator->getActualQM(), handle->expectedBitRate);

        handle->translator->setTargetQM(targetQM);

        EV_DEBUG << "CoCC applying target QM of period " << handle->feedback.period << ", originally computed: " << handle->feedback.targetQM << ", now: " << targetQM << endl;

        emit(s_expectedRate, handle->expectedBitRate);
        emit(s_appliedQM, targetQM);
    }

    // commit ticker is rescheduled when push response is received
}

bool CoCCUDPTransport::coccRateLimitAccept(SocketHandle_t * const handle, cPacket * const pkt) {
    ASSERT(handle);
    ASSERT(pkt);

    const simtime_t now = simTime();

    handle->burstCredit += (now - handle->lastCreditUpdate).dbl() * handle->expectedBitRate;
    handle->burstCredit = std::min(handle->burstCredit, permittedBurstSize * 8L);
    handle->lastCreditUpdate = now;

    long expectedPktLength = pkt->getBitLength() + lowerLayerOverhead * 8;

    if (handle->burstCredit > expectedPktLength) {
        handle->burstCredit -= expectedPktLength;

        return true;
    } else {
        return false;
    }
}

void CoCCUDPTransport::postControlStep(void * const context) {
    SocketHandle_t * const handle = (SocketHandle_t *)context;

    if (handle->connected && handle->inactivityCounter < COCC_INACTIVITY_STOP ) {
        ASSERT(handle->role == NCTXCI_CONTROLLER);
        ASSERT(handle->translator);

        const double actualQM = handle->translator->getActualQM();
        const double targetQM = handle->translator->getTargetQM();
        const double actualRate = handle->translator->getRateForQM(actualQM, targetQM);
        const double delta = std::abs(handle->expectedBitRate - actualRate);


        if (delta > handle->expectedBitRate * 0.005) { // ignore < 0.5% deviation
            const double adjustedTargetQM = handle->translator->getQMForRate(targetQM, handle->expectedBitRate);

            EV_DEBUG << "CoCC readjusting qmTarget to track expected rate. "
                    << "Expected=" << handle->expectedBitRate << " actual=" << actualRate << " New qmTarget=" << adjustedTargetQM << endl;

            handle->translator->setTargetQM(adjustedTargetQM);
            emit(s_appliedQM, adjustedTargetQM);
        }
    }
}

CoCCUDPTransport::SocketHandle_t * CoCCUDPTransport::createSocket() {
    SocketHandle_t * const handle = new SocketHandle_t();

    handle->socket.reset(new UDPSocket());
    handle->socket->setOutputGate(udpOut);

    handle->burstCredit = permittedBurstSize * 8;
    handle->lastCreditUpdate = simTime();

    if (pushCompensationHorizon > 0) {
        handle->forcedPushHistory = RunningWindowStats<double>(pushCompensationHorizon);
        handle->expectedPushHistory = RunningWindowStats<double>(pushCompensationHorizon);
    }

    return handle;
}

void CoCCUDPTransport::connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port) {
    ASSERT(handle);

    handle->socket->connect(endpAddr, port);

    handle->listening = false;
    handle->connected = false;
    handle->remote = endpAddr;
    handle->port = port;
}

void CoCCUDPTransport::storeSocket(SocketHandle_t * const handle) {
    ASSERT(handle);

    // may fail for incoming connections tied to the listen socket
    // thats ok, we use the map in first place to retrieve the listen socket or
    // to check if a socket context has been established. thus no assert here
    connectionMap.insert(SocketMap_t::value_type(handle->socket->getSocketId(), handle));

    connectionVect.insert(connectionVect.end(), handle);
}

void CoCCUDPTransport::initHandshake(SocketHandle_t * const handle, cMessage * const selfMsg) {
    // send connect notification to server
    UDPHandshake* const hs = new UDPHandshake("Connect Handshake");

    hs->setSynAck(false);

    handle->socket->send(hs);

    scheduleAt(simTime() + 0.25, selfMsg);
}

/**
 * Returns either the listen socket which might be used for several incoming
 * connections, or the socket associated with an outgoing connection.
 */
CoCCUDPTransport::SocketHandle_t * CoCCUDPTransport::getSocketById(const int id) {
    const auto it = connectionMap.find(id);

    if (it == connectionMap.end()) {
        return nullptr;
    }

    SocketHandle_t * const handle = it->second;
    ASSERT(it->first == handle->socket->getSocketId());

    return handle;
}

/**
 * Returns an existing socket which is connected to the remote address.
 * If multiple sockets with the same remote address exist, the one with a remote
 * port equal to the passed one is preferred.
 * Even if this mechanism in theory supports more than one connection between
 * client and server, its purpose is to enable server->client routing without
 * requiring the server to know the dst port of the client.
 */
CoCCUDPTransport::SocketHandle_t * CoCCUDPTransport::getSocketByAddr(const L3Address& addr, const uint16_t port) {
    SocketHandle_t * handle = nullptr;

    for (auto it = connectionVect.begin(); it != connectionVect.end(); it++) {
        if ((*it)->remote == addr) {
            handle = *it; // address match, but a perfect one might be possible

            if (handle->port == port) {
                break; // perfect match, do not continue
            }
        }
    }

    return handle;
}
