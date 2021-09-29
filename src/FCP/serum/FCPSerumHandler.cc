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

#include "FCP/serum/FCPSerumHandler.h"

#include "FCP/serum/FCPSerumHeader_m.h"

Define_Module(FCPSerumHandler);

#define TICKER_MSG_KIND 9025 // randomly chosen


FCPSerumHandler::FCPSerumHandler() {

}

FCPSerumHandler::~FCPSerumHandler() {

}

void FCPSerumHandler::initialize() {
    collectionInterval = par("collectionInterval").doubleValue();
    averagingWindow = par("averagingWindow").doubleValue();
    alpha = par("alpha").doubleValue();

    utilizationThreshold = par("utilizationThreshold").doubleValue();

    minPrice = par("minPrice").doubleValue();

    enableItSmoothing = par("enableItSmoothing").boolValue();
    itHistoryLength = par("itHistoryLength").intValue();

    tickerEvent = new cMessage("FCP SERUM cyclic processing ticker event", TICKER_MSG_KIND);
    scheduleAt(collectionInterval, tickerEvent);

    priceSignal = registerSignal("price");
    i_tSignal = registerSignal("i_t");
    sumSignal = registerSignal("sum");

    rttSignal = registerSignal("rtt");
    matchingPriceSignal = registerSignal("matchingPrice");
    packetSizeSignal = registerSignal("packetSize");
    packetValueSignal = registerSignal("packetValue");
    remainingLinkCapacitySignal = registerSignal("remainingLinkCapacity");
    avgQueueLengthSignal = registerSignal("avgQueueLength");

    lastPacketsSizeSignal = registerSignal("lastPacketsSize");

    avgQMSignal = registerSignal("avgQM");
    avgBudgetSignal = registerSignal("avgBudget");
    utilizationSignal = registerSignal("utilization");

    packetArrivedSignal = registerSignal("packetArrived");
    oldestPacketDiffSignal = registerSignal("oldestPacketDiff");

    originalQMSignal = registerSignal("originalQM");
    scaledQMSignal = registerSignal("scaledQM");
}

void FCPSerumHandler::handleMessage(cMessage * const msg) {
    switch (msg->getKind()) {
        case TICKER_MSG_KIND:
            for (auto iter = ifaces.begin(); iter != ifaces.end(); iter++) {

                EV_INFO << "Collection finished for interface " << iter->ie->getName() << endl;
                EV_INFO << "flows: " << iter->qmAccumulator.flows << ", qm_sum: " << iter->qmAccumulator.qm_sum << ", budget_sum: " << iter->qmAccumulator.budget_sum << endl;

                iter->concludeCollection(utilizationThreshold);

                if (mc) {
                    MonitoringCollector::Statistics stats = mc->getStatistics(iter->ie->getInterfaceModule());

                    EV_INFO << "avg util: " << stats.avgUtilization << endl;

                    iter->utilization = stats.avgUtilization;

                    if (stats.avgUtilization >= 0.05) {
                        emit(utilizationSignal, stats.avgUtilization);
                        EV_INFO << "avgQM: " << iter->qmCollected.averageQM << ", avgBudget: " << iter->qmCollected.averageBudget << endl;
                        EV_INFO << "qmHistory size: " << iter->collectedQMHistory.size() << " flowHistory size: " << iter->flowsHistory.size() << endl;

                    }

                    if (stats.avgUtilization >= 0.05) {
                        if ((iter->qmCollected.averageQM) != -1) {
                            emit(avgQMSignal, iter->qmCollected.averageQM);
                            emit(avgBudgetSignal, iter->qmCollected.averageBudget);

                            if (iter->qmCollected.originalQM != -1) {
                                emit(originalQMSignal, iter->qmCollected.originalQM);
                                emit(scaledQMSignal, iter->qmCollected.scaledQM);
                            }

                        }
                    }

                }
            }

            scheduleAt(simTime() + collectionInterval, msg);

            EV_INFO << "FCP SERUM monitoring collection concluded" << endl;
            break;
        default:
            const int msgKind = msg->getKind();

            delete msg;

            error("Received message with unexpected message kind: %i", msgKind);
            break;
    }
}

std::vector<SerumSupport::DatasetInfo> FCPSerumHandler::datasetDescriptors() {
    const SerumSupport::DatasetInfo pushInfo { DATASET_FCP_PUSH, false }; // pushed information is tied to outbound interface, hence reverse==false
    const SerumSupport::DatasetInfo respInfo { -DATASET_FCP_RESP, true }; // to collect information tied to inbound interface, hence reverse==true

    return std::vector<SerumSupport::DatasetInfo> { pushInfo, respInfo };
}

void* FCPSerumHandler::interfaceAdded(const InterfaceEntry * const ie, const short dataDesc) {
    EV_INFO << "new interface " << ie->getName() << endl;

    for (auto it = ifaces.begin(); it != ifaces.end(); it++) {
        if (it->interfaceId == ie->getInterfaceId()) {
            EV_INFO << "already known" << endl;

            // we don't care about the dataDesc, push and resp operate on the same data
            return &(*it);
        }
    }

    ifaces.push_back(FCPSerumHandler::InterfaceData(ie));

    return &ifaces.back();
}

void FCPSerumHandler::handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    InterfaceData * const id = static_cast<InterfaceData *>(handlerData);

    ASSERT(id);

    FCPResponseRecord * rr = new FCPResponseRecord();

    EV_INFO << "Received FCPResponseRecord for interface " << id->ie->getName() << endl;
    EV_INFO << "collected avgQM: " << id->qmCollected.averageQM << ", prev collected avgQM: " << id->collectedQMHistory.back() << ", flows: " << id->qmCollected.flows << endl;

    rr->setAverageBottleneckQM(id->qmCollected.averageQM);
    rr->setAverageBottleneckBudget(id->qmCollected.averageBudget);
    rr->setPrevAvgBottleneckQM(id->collectedQMHistory.back());
    rr->setFlows(id->qmCollected.flows);

    EV_INFO << "Updating FCPResponseRecord: avgBNQM: " << id->qmCollected.averageQM << ", prevAvgQM: " << id->collectedQMHistory.back() << ", avgBNBudget: " << id->qmCollected.averageBudget << ", flows: "
            << id->qmCollected.flows << endl;

    opts->add(rr);
}

void FCPSerumHandler::handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    InterfaceData * const id = static_cast<InterfaceData *>(handlerData);

    ASSERT(id);

    FCPPushRecord * const pr = dynamic_cast<FCPPushRecord *>(&opts->getTlvOption(optIndex));

    ASSERT(pr);

    EV_INFO << "Received FCPPushRecord for interface " << id->ie->getName() << endl;

    simtime_t t = simTime();
    double rtt = pr->getRtt();
    double preload = pr->getPreload();
    double size = 1.0 * pr->getSize();

    int count = id->lastPackets.removePackets(t, averagingWindow);

    EV_DETAIL << "Removed " << count << " packets at time: " << t << endl;

    if (!mc){
        error("MonitoringCollector not available");
    }

    MonitoringCollector::Statistics statis = mc->getStatistics(id->ie->getInterfaceModule());
    MonitoringCollector::QueueStatistics qStats;

    for (auto q : statis.queues) {
        if (opp_strcmp("queue", q.name) || opp_strcmp("BE", q.name)) {
            qStats = q;
            break;
        }
    }

    if(rtt > 0){
        double lineRate = statis.lineRate;
        double matchingPrice = id->lastPackets.findMatchingPrice(t.dbl(), rtt);

        if (matchingPrice == -1) {
            matchingPrice = minPrice;
        }

        double packetValue = size * (1 + preload * (averagingWindow.dbl()/rtt));
        double sum = id->lastPackets.getSum();
        double i = (packetValue * matchingPrice + sum)/averagingWindow.dbl();
        double remainingLinkCapacity = lineRate - (alpha * qStats.avgLength * packetValue / averagingWindow.dbl());

        if (enableItSmoothing) {
            id->addItValue(i, itHistoryLength);
            i = id->getAverageIt();
        }


        if (remainingLinkCapacity < lineRate / 4) {
            remainingLinkCapacity = lineRate / 4;
        }

        double price = i/(remainingLinkCapacity);

        EV_INFO << "lineRate: " << lineRate << ", RTT: " << rtt << ", packetSize: " << size << ", packetValue: " << packetValue << ", sum: " << sum << endl;
        EV_INFO << "i(t): " << i << ", remainingLinkCapacity: " << remainingLinkCapacity << ", avgQueueLength: " << qStats.avgLength << ", price: " << price << endl;



        if (price < minPrice) {
            price = minPrice;
        }

        id->lastPackets.addPacket(t, matchingPrice, price, packetValue);

        // price + minPrice = 0
        // therefore only update if price != minPrice
        if (price != minPrice) {
            pr->setPrice(pr->getPrice() + price);
        }

        EV_INFO << "lastPackets size: " << id->lastPackets.packets.size() << endl;

        EV_INFO << "time now: " << simTime() << ", oldest packet: " << id->lastPackets.packets.front().arrival << ", newest packet: " << id->lastPackets.packets.back().arrival << endl;

        simtime_t oldestPacketDiff = t - id->lastPackets.packets.front().arrival;

        EV_INFO << "difference now oldest: " << oldestPacketDiff << endl;

        if (packetValue == 3424 && id->utilization >= 0.05) {
            emit(priceSignal, price);
            emit(i_tSignal, i);
            emit(sumSignal, sum);
            emit(rttSignal, rtt);
            emit(matchingPriceSignal, matchingPrice);
            emit(packetSizeSignal, size);
            emit(packetValueSignal, packetValue);
            emit(remainingLinkCapacitySignal, remainingLinkCapacity);
            emit(avgQueueLengthSignal, qStats.avgLength);
            emit(lastPacketsSizeSignal, (double)id->lastPackets.packets.size());
            emit(packetArrivedSignal, true);
            emit(oldestPacketDiffSignal, oldestPacketDiff.dbl());
        }


        // qm collection
        if (pr->getCollectQM()) {

            EV_INFO << "Collecting qm and budget allowed" << endl;

            double util = id->utilization;



            //if (util >= utilizationThreshold) {

                EV_INFO << "Utilization of: " << util << " is above threshold: " << utilizationThreshold << endl;
                EV_INFO << "flows: " << pr->getFlows() << ", pushed BN QM: " << pr->getBottleneckQM() << ", old collected QM: " << id->collectedQMHistory[1] << endl;

                //if (pr->getBottleneckQM() == -1 || (pr->getBottleneckQM() == id->collectedQMHistory[1] && pr->getFlows() == id->flowsHistory.back()) || id->collectedQMHistory[1] == -1) {
                if ((pr->getBottleneckQM() >= id->collectedQMHistory[1]) || pr->getBottleneckQM() == -1) {
                    id->qmAccumulator.add(pr->getTargetQM(), pr->getBudget());
                    EV_INFO << "Collected targetQM: " << pr->getTargetQM() << " and budget: " << pr->getBudget() << endl;
                }
            //} else {
                EV_INFO << "Utilization too low: " << util << ", threshold: " << utilizationThreshold << endl;
            //}

        }


        EV_INFO << "Calculated new price: " << pr->getPrice() << endl;

    } else {
        EV_INFO << "Couldn't calculate a new price because the rtt is null or negative." << endl;
    }
}


FCPSerumHandler::InterfaceData::InterfaceData(const InterfaceEntry * const ie) :
        ie(ie),
        interfaceId(ie->getInterfaceId()) {
    collectedQMHistory.push_front(-1);
    collectedQMHistory.push_front(-1);
    collectedQMHistory.push_front(-1);
    flowsHistory.push_front(-1);
    flowsHistory.push_front(-1);
}

void FCPSerumHandler::InterfaceData::concludeCollection(double utilThr) {
    qmCollected = qmAccumulator;
    qmCollected.calculateAverage(utilization, utilThr);
    qmAccumulator.reset();

    collectedQMHistory.push_front(qmCollected.averageQM);
    collectedQMHistory.pop_back();
    flowsHistory.push_front(qmCollected.flows);
    flowsHistory.pop_back();
}

void FCPSerumHandler::InterfaceData::addItValue(double it, int historyLength) {
    itHistory.push_back(it);

    while (itHistory.size() > historyLength) {
        itHistory.pop_front();
    }
}

double FCPSerumHandler::InterfaceData::getAverageIt() {
    double sum = 0;

    for (std::list<double>::iterator it = itHistory.begin(); it != itHistory.end(); it++) {
        sum += *it;
    }

    return sum / itHistory.size();
}

FCPSerumHandler::PacketData::PacketData(simtime_t t, double matchedP, double calcP, double v) :
    arrival(t), matchedPrice(matchedP), calculatedPrice(calcP), value(v) {

}

FCPSerumHandler::FCPQMData::FCPQMData() :
        budget_sum(0), qm_sum(0), averageBudget(-1), averageQM(-1), flows(0), scaledQM(-1), originalQM(-1) {
}

void FCPSerumHandler::FCPQMData::add(const double q, const double b) {
    qm_sum += q;
    budget_sum += b;
    flows++;
}

void FCPSerumHandler::FCPQMData::calculateAverage(double util, double utilThr) {

    if (flows > 0) {
        averageBudget = budget_sum / flows;
        averageQM = qm_sum / flows;

        if (util > 0.2 && util < utilThr) {

            originalQM = averageQM;

            double m = (averageQM - 1) / (utilThr - 0.2);
            double c = 1 - m * 0.2;

            averageQM = m * util + c;
            scaledQM = averageQM;

        } else if (util <= 0.2) {
            averageQM = 1;
        }
    }

}

void FCPSerumHandler::FCPQMData::reset() {
    qm_sum = budget_sum = 0;
    averageBudget = -1;
    averageQM = -1;
    flows = 0;
    scaledQM = -1;
    originalQM = -1;
}

FCPSerumHandler::FCPData::FCPData(){

}

void FCPSerumHandler::FCPData::addPacket(simtime_t t, double matchedPrice, double calculatedPrice, double value){
    packets.push_back(PacketData(t, matchedPrice, calculatedPrice, value));
}

int FCPSerumHandler::FCPData::removePackets(simtime_t t, simtime_t window){
    int count = 0;
    for (std::list<PacketData>::iterator it = packets.begin(); it != packets.end();){

        simtime_t diff = t - it->arrival;
        if(diff >= window) {
            it = packets.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    return count;
}

double FCPSerumHandler::FCPData::findMatchingPrice(simtime_t t, double rtt){
    double diff;
    double price = -1;
    for (std::list<PacketData>::iterator it = packets.begin(); it != packets.end(); ++it){
        if(price == -1 || fabs(t.dbl() - rtt - it->arrival.dbl()) < diff){
            price = it->calculatedPrice;
            diff = fabs(t.dbl() - rtt - it->arrival.dbl());
        }
    }
    if(price == -1){
        price = -1;
    }
    return price;
}

double FCPSerumHandler::FCPData::getSum(){
    double value = 0;

    for (std::list<PacketData>::iterator it = packets.begin(); it != packets.end(); ++it){
        value += it->matchedPrice * it->value;
    }

    return value;
}
