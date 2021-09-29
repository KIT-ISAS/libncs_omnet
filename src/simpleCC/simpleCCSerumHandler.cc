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

#include <simpleCC/simpleCCSerumHandler.h>
#include <simpleCC/simpleCCUDPTransport.h>
#include "simpleCCSerumHeader_m.h"

Define_Module(simpleCCSerumHandler);


simpleCCSerumHandler::simpleCCSerumHandler() {

}

simpleCCSerumHandler::~simpleCCSerumHandler() {

}

void simpleCCSerumHandler::initialize() {
}

void simpleCCSerumHandler::handleMessage(cMessage * const msg) {
    switch (msg->getKind()) {
        default:
            const int msgKind = msg->getKind();

            delete msg;

            error("Received message with unexpected message kind: %i", msgKind);
            break;
    }
}

std::vector<SerumSupport::DatasetInfo> simpleCCSerumHandler::datasetDescriptors() {
    const SerumSupport::DatasetInfo pushInfo { DATASET_simpleCC_PUSH, false }; // tie pushed information to outbound interface
                                                                               // since we are also interested in monitoring information of this interface
    const SerumSupport::DatasetInfo respInfo { -DATASET_simpleCC_RESP, true }; // initial response record is sent with inverted descriptor
                                                                               // pushed information is tied to inbound interface, hence reverse==true

    return std::vector<SerumSupport::DatasetInfo> { pushInfo, respInfo };
}

void* simpleCCSerumHandler::interfaceAdded(const InterfaceEntry * const ie, const short dataDesc) {
    for (auto it = ifaces.begin(); it != ifaces.end(); it++) {
        if (it->interfaceId == ie->getInterfaceId()) {
            // we don't care about the dataDesc, push and resp operate on the same data
            return &(*it);
        }
    }

    ifaces.push_back(simpleCCSerumHandler::InterfaceData(ie));

    return &ifaces.back();
}

void simpleCCSerumHandler::handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    InterfaceData * const id = static_cast<InterfaceData *>(handlerData);

    ASSERT(id);

    simpleCCResponseRecord * rr = new simpleCCResponseRecord();

    if (!mc){
        error("MonitoringCollector not available");
    }

    const MonitoringCollector::Statistics stats = mc->getStatistics(id->ie->getInterfaceModule());

    double queueLength = 0;
    long queueSize = 0;

    for (auto q : stats.queues) {
        if (!opp_strcmp("queue", q.name)
                || !opp_strcmp("ctrlEF", q.name)
                || !opp_strcmp("ctrlPriority", q.name)
                || !opp_strcmp("ctrlLBE", q.name)) {
            queueLength += q.avgLength.filtered();
            queueSize += q.size;
        }
    }

    //collect data from router
    rr->setLineRate(stats.lineRate);
    rr->setQueueSize(queueSize);
    rr->setAvgQueueLength(queueLength);
    rr->setAvgUtilization(stats.avgUtilization);

    opts->add(rr);

    // FIXME-SERUM: datagram pkt size is not adjusted

    EV_INFO << "Providing simpleCC response data for interface " << id->ie->getInterfaceModule()->getFullPath() << endl;
}

void simpleCCSerumHandler::handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    // nothing to do here yet, since simpleCC does not push data to routers
}


simpleCCSerumHandler::InterfaceData::InterfaceData(const InterfaceEntry * const ie) :
        ie(ie),
        interfaceId(ie->getInterfaceId()) {

}
