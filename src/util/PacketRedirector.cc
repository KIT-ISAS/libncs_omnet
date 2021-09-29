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

#include "PacketRedirector.h"

#include "TransportCtrlMsg.h"

#include <inet/common/InitStages.h>
#include <inet/networklayer/common/L3AddressResolver.h>

#include <string>

Define_Module(PacketRedirector);

int PacketRedirector::numInitStages() const {
    return INITSTAGE_LAST;
}

void PacketRedirector::initialize(const int stage) {
    switch (stage) {
    case INITSTAGE_LOCAL:
        upIn = gate("up$i");
        upOut = gate("up$o");
        downIn = gate("down$i");
        downOut = gate("down$o");

        tOpen = par("tOpen");
        tClose = par("tClose");

        connectAddress = par("connectAddress").stdstringValue();
        connectPort = par("connectPort");
        localPort = par("localPort");
        break;
    case INITSTAGE_APPLICATION_LAYER: {
        // IP auto assignment, address by name

        if (localPort > 0) {
            ASSERT(localPort < UINT16_MAX);

            cMessage * const msg = new cMessage(("PacketRedirector listen at " + std::to_string(localPort)).c_str());
            TransportListenReq * const lmsg = new TransportListenReq();

            lmsg->setListenPort(localPort);
            msg->setControlInfo(lmsg);

            send(msg, downOut);
        }

        if (connectPort > 0) {
            if (tOpen != SIMTIME_ZERO) {
                cMessage * const msg = new cMessage("PacketRedirector tOpen delay");

                scheduleAt(tOpen, msg);
            } else {
                initiateConnection();
            }
        }

        break; }
    }
}

void PacketRedirector::handleMessage(cMessage * const msg) {
    if (msg->isSelfMessage()) {
        initiateConnection();

        delete msg;

        return;
    }

    if (simTime() <= tOpen || (tClose >= 0 && simTime() >= tClose)) {
        EV << "Closed. Dropping msg " << msg << endl;

        delete msg;

        return;
    }

    if (msg->arrivedOn(upIn->getId())) {
        cObject * const oldCtrl = msg->removeControlInfo();

        if (oldCtrl != nullptr) {
            delete oldCtrl;
        }

        TransportDataInfo * const ctrl = new TransportDataInfo();

        ctrl->setDstAddr(connectL3Addr);
        ctrl->setDstPort(connectPort);

        msg->setControlInfo(ctrl);

        send(msg, downOut);
    } else if (msg->arrivedOn(downIn->getId())) {
        if (dynamic_cast<TransportDataInfo *>(msg->getControlInfo())) {
            send(msg, upOut);
        } else {
            EV << "Dropping control msg " << msg << endl;

            delete msg;
        }
    } else {
        error("Unable to handle message from unknown source.");
    }
}

void PacketRedirector::initiateConnection() {
    L3AddressResolver().tryResolve(connectAddress.c_str(), connectL3Addr,
            L3AddressResolver::ADDR_IPv4 | L3AddressResolver::ADDR_IPv6);

    if (connectL3Addr.isUnspecified()) {
        error(("Unable to resolve address for " + connectAddress).c_str());
    }

    if (connectL3Addr.isLinkLocal()) {
        error("Network was not fully set up during initialization, PacketRedirector is unable to operate. "
                "Choose parameter tOpen sufficiently large such that all hosts got an network address assigned");
    }

    cMessage* const msg = new cMessage(
            ("PacketRedirector connect to " + connectAddress + ":"
                    + std::to_string(connectPort)).c_str());
    TransportConnectReq* const cmsg = new TransportConnectReq();

    cmsg->setDstAddr(connectL3Addr);
    cmsg->setDstPort(connectPort);
    msg->setControlInfo(cmsg);

    send(msg, downOut);
}
