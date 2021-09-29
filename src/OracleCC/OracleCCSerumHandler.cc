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

#include "OracleCCSerumHandler.h"

#include "OracleCCSerumHeader_m.h"

Define_Module(OracleCCSerumHandler);

void OracleCCSerumHandler::initialize() {
}

void OracleCCSerumHandler::handleMessage(cMessage * const msg) {
    const int msgKind = msg->getKind();

    delete msg;

    error("Received message with unexpected message kind: %i", msgKind);
}

std::vector<SerumSupport::DatasetInfo> OracleCCSerumHandler::datasetDescriptors() {
    const SerumSupport::DatasetInfo inboundRecord { DATASET_OCC_DISCOVER_INBOUND, true };
    const SerumSupport::DatasetInfo outboundRecord { DATASET_OCC_DISCOVER_OUTBOUND, false };

    return std::vector<SerumSupport::DatasetInfo> { inboundRecord, outboundRecord };
}

void* OracleCCSerumHandler::interfaceAdded(const InterfaceEntry * const ie, const short dataDesc) {
    return const_cast<InterfaceEntry *>(ie); // its OK because we do not change anything
}

void OracleCCSerumHandler::handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    error("No handler registered for append records");
}

void OracleCCSerumHandler::handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex) {
    Enter_Method_Silent();

    const InterfaceEntry * const ie = static_cast<const InterfaceEntry *>(handlerData);

    ASSERT(ie);
    ASSERT(dynamic_cast<OracleCCDiscoverRecord *>(&opts->getTlvOption(optIndex)));

    OracleCCDiscoverRecord * const dr = reinterpret_cast<OracleCCDiscoverRecord *>(&opts->getTlvOption(optIndex));

    OracleCCCoordinator::addEdge(this, ie, dr->getSrc(), dr->getHandle(), dr->getDataDesc() == DATASET_OCC_DISCOVER_INBOUND);
}

MonitoringCollector::Statistics OracleCCSerumHandler::getMonitoringStatistics(const InterfaceEntry * const ie) {
    ASSERT(ie);

    return mc->getStatistics(ie->getInterfaceModule());
}
