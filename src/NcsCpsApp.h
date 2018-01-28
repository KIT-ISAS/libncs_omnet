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

#ifndef __LIBNCS_OMNET_NCSCPSAPP_H_
#define __LIBNCS_OMNET_NCSCPSAPP_H_

#include <omnetpp.h>
#include <inet/transportlayer/contract/tcp/TCPSocket.h>
#include <inet/common/ByteArray.h>
#include "messages/NcsCtrlMsg_m.h"

using namespace omnetpp;
using namespace inet;

enum NcspCpsAppMessageKind_t {
    CpsConnReq = 2300,
    CpsSendData
};

class NcsCpsApp: public cSimpleModule {
public:
    NcsCpsApp();
    virtual ~NcsCpsApp() {};

protected:
    virtual int numInitStages() const;
    virtual void initialize(const int stage);
    virtual void handleMessage(cMessage * const msg);

private:
    bool doListen;
    uint16_t connectPort;

    cGate * ctxIn;
    cGate * ctxOut;
    cGate * transportIn;
    cGate * transportOut;

    // statistical data
    simsignal_t interArrivalTimeSignal;
    simtime_t lastPktTime = SimTime::ZERO;
};

#endif
