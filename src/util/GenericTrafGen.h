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

#ifndef __LIBNCS_OMNET_TRAFGEN_H_
#define __LIBNCS_OMNET_TRAFGEN_H_

#include <omnetpp.h>

#include <inet/common/INETDefs.h>

#include <inet/common/lifecycle/ILifecycle.h>
#include <inet/common/lifecycle/NodeStatus.h>

using namespace inet;
using namespace omnetpp;

// This class is derived from inet/applications/generic/IPvXTrafGen
class GenericTrafGen : public cSimpleModule, public ILifecycle
{
  protected:
    enum Kinds { START = 100, NEXT };

    // parameters: see the NED files for more info
    bool generateRaw;
    simtime_t startTime;
    simtime_t stopTime;
    cPar *sendIntervalPar = nullptr;
    cPar *packetLengthPar = nullptr;
    int numPackets = 0;

    // gates
    cGate * inGate;
    cGate * outGate;

    // state
    NodeStatus *nodeStatus = nullptr;
    cMessage *timer = nullptr;
    bool isOperational = false;

    // statistic
    int numSent = 0;
    int numReceived = 0;
    static simsignal_t sentPkSignal;
    static simsignal_t rcvdPkSignal;

  public:
    GenericTrafGen();
    virtual ~GenericTrafGen();

  protected:
    virtual void scheduleNextPacket(simtime_t previous);
    virtual void cancelNextPacket();
    virtual bool isEnabled();

    virtual void sendPacket();

    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    virtual void startApp();

    virtual void printPacket(cPacket *msg);
    virtual void processPacket(cPacket *msg);
    virtual bool handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback) override;
};

#endif
