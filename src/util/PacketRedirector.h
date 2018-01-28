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

#ifndef __LIBNCS_OMNET_PACKETREDIRECTOR_H_
#define __LIBNCS_OMNET_PACKETREDIRECTOR_H_

#include <omnetpp.h>

#include <inet/common/RawPacket.h>
#include <inet/networklayer/common/L3Address.h>

using namespace omnetpp;
using namespace inet;

class PacketRedirector : public cSimpleModule {
  protected:
    // parameters
    std::string connectAddress;
    int connectPort = 1000;
    int localPort = 1000;  // local port
    simtime_t tOpen = 0;
    simtime_t tClose = -1;

    // vars
    cGate *upIn;
    cGate *upOut;
    cGate *downIn;
    cGate *downOut;

    L3Address connectL3Addr;
  protected:
    virtual int numInitStages() const;
    virtual void initialize(const int stage);
    virtual void handleMessage(cMessage *msg);
};

#endif
