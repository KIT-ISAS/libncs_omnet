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

#include "util/TransportCtrlMsg_m.h"
#include "messages/NcsCtrlMsg_m.h"

#include <inet/common/InitStages.h>
#include <inet/common/RawPacket.h>
#include <NcsCpsApp.h>

Define_Module(NcsCpsApp);

NcsCpsApp::NcsCpsApp() {
    // nop
}

int NcsCpsApp::numInitStages() const {
    return NUM_INIT_STAGES;
}

void NcsCpsApp::initialize(const int stage) {
    switch (stage) {
    case INITSTAGE_LOCAL:
        ctxIn = gate("context$i");
        ctxOut = gate("context$o");
        transportIn = gate("transport$i");
        transportOut = gate("transport$o");

        doListen = par("doListen");
        connectPort = par("connectPort");
        interArrivalTimeSignal = registerSignal("inter_arrival_t");
        break;
    case INITSTAGE_LAST:
        if (doListen) {
            cMessage * const msg = new cMessage("CPS Listen");
            TransportListenReq * const req = new TransportListenReq();

            req->setListenPort(connectPort);
            msg->setControlInfo(req);

            send(msg, transportOut);
        }
        break;
    }
}

void NcsCpsApp::handleMessage(cMessage * const msg) {
    if (msg->arrivedOn(transportIn->getId())) {
        RawPacket * const rawPkt = dynamic_cast<RawPacket *>(msg);
        TransportDataInfo * const info = dynamic_cast<TransportDataInfo *>(msg->removeControlInfo());

        ASSERT(rawPkt);
        ASSERT(info);

        NcsSendData * const req = new NcsSendData();

        req->setSrcAddr(info->getSrcAddr());
        req->setDstAddr(info->getDstAddr());

        rawPkt->setControlInfo(req);

        send(rawPkt, ctxOut);

        delete info;
    } else if (msg->arrivedOn(ctxIn->getId())) {
        switch (msg->getKind()) {
        case CpsConnReq: {
            NcsConnReq * const req = dynamic_cast<NcsConnReq *>(msg->removeControlInfo());

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected NcsConnReq control info in message.");
            }

            TransportConnectReq * const tReq = new TransportConnectReq();

            tReq->setDstAddr(req->getDstAddr());
            tReq->setDstPort(connectPort);
            msg->setControlInfo(tReq);

            send(msg, transportOut);

            delete req;
            break;
        }
        case CpsSendData: {
            ASSERT(dynamic_cast<RawPacket *>(msg));

            NcsSendData * const req = dynamic_cast<NcsSendData *>(msg->removeControlInfo());

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected NcsSendData control info in message.");
            }

            TransportDataInfo * const info = new TransportDataInfo();

            info->setSrcAddr(req->getSrcAddr());
            info->setDstAddr(req->getDstAddr());
            // srcPort is unknown, to be filled in on reception
            // dstPort may be wrong! only valid if this CPS is the client
            // dstPort is unknown to the server since it is chosen by the client
            // TODO: maybe establish some (transparent) connection context at App-Layer?
            info->setDstPort(connectPort);
            msg->setControlInfo(info);

            send(msg, transportOut);

            delete req; // ControlInfo from NcsContext will be reconstructed on reception again

            // collect statistics
            emit(interArrivalTimeSignal, simTime() - lastPktTime);

            lastPktTime = simTime();

            break;
        }
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
