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

#ifndef __LIBNCS_OMNET_ORACLECCSERUMHANDLER_H_
#define __LIBNCS_OMNET_ORACLECCSERUMHANDLER_H_

#include <omnetpp.h>

#include <memory>

#include <inet/networklayer/serum/SerumSupport.h>

#include "OracleCCCoordinator.h"

using namespace omnetpp;
using namespace inet;

class OracleCCSerumHandler : public SerumSupport::RecordHandler, public cSimpleModule {

  public:

    OracleCCSerumHandler() { };
    virtual ~OracleCCSerumHandler() { };
    virtual std::vector<SerumSupport::DatasetInfo> datasetDescriptors();
    virtual void* interfaceAdded(const InterfaceEntry * const ie, const short dataDesc);
    virtual void handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex);
    virtual void handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex);

    MonitoringCollector::Statistics getMonitoringStatistics(const InterfaceEntry * const ie);

  protected:

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

};

#endif
