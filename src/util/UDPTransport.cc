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

#include "UDPTransport.h"

#include "UDPHandshakePkt_m.h"

#include <NcsCpsApp.h>

Define_Module(UDPTransport);

UDPTransport::UDPTransport() {

}

UDPTransport::~UDPTransport() {
    // connectionVect contains each created handle, even the listening one
    for (auto handle : connectionVect) {
        delete handle;
    }
}

void UDPTransport::initialize() {
    udpIn = gate("udpIn");
    udpOut = gate("udpOut");
    upIn = gate("up$i");
    upOut = gate("up$o");
}

void UDPTransport::handleMessage(cMessage * const msg) {
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
                    TransportDataInfo * const info = new TransportDataInfo();

                    info->setSrcAddr(ctrl->getSrcAddr());
                    info->setSrcPort(ctrl->getSrcPort());
                    info->setDstAddr(ctrl->getDestAddr());
                    info->setDstPort(ctrl->getDestPort());
                    info->setNetworkOptions(ctrl->replaceNetworkOptions());

                    msg->setControlInfo(info);

                    send(msg, upOut);
                } else {
                    EV_WARN << "No connection to " << ctrl->getSrcAddr() << " has been established yet. Dropping message: " << msg << endl;

                    delete msg;
                }
            } else {
                UDPHandshake * const hs = dynamic_cast<UDPHandshake *>(msg);

                if (!hs->getSynAck()) {
                    // listening part, create dedicated handle for sending packets from a listening host
                    SocketHandle_t * const txHandle = new SocketHandle_t();

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

                    // confirm connection to upper layer
                    auto conf = new TransportConnectReq();

                    msg->setName("Connection confirmation");
                    msg->setKind(CpsConnReq);

                    conf->setDstAddr(handle->remote);
                    conf->setDstPort(handle->port);
                    msg->setControlInfo(conf);

                    send(msg, upOut);
                }
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

            SocketHandle_t * const handle = createSocket();

            connectSocket(handle, req->getDstAddr(), req->getDstPort());
            storeSocket(handle);

            // timeout notification for retries
            cMessage * const selfMsg = new cMessage("Connect Timeout");
            selfMsg->setContextPointer(handle);

            // send connect notification to server and start timeout
            initHandshake(handle, selfMsg);

            delete msg;
        } else if (dynamic_cast<TransportListenReq *>(ctrlInfo)) {
            const uint16_t listenPort = dynamic_cast<TransportListenReq *>(ctrlInfo)->getListenPort();

            delete msg;

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
        } else if (dynamic_cast<TransportDataInfo *>(ctrlInfo)) {
            TransportDataInfo * const req = dynamic_cast<TransportDataInfo *>(msg->removeControlInfo());

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportDataInfo control info in message.");
            }
            ASSERT(dynamic_cast<cPacket *>(msg));

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                ASSERT(req->getDstAddr() == handle->remote);

                if (handle->connected) {
                    inet::UDPSocket::SendOptions opts;

                    opts.networkOptions = req->replaceNetworkOptions();

                    handle->socket->sendTo(dynamic_cast<cPacket *>(msg), handle->remote, handle->port, &opts);
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
            EV_WARN << "Received message with unknown control info type, ignoring: " << msg << endl;

            delete msg;
        }
    } else if (msg->isSelfMessage()) {
        // Timeout-Ticker
        SocketHandle_t * const handle = (SocketHandle_t *)msg->getContextPointer();

        ASSERT(handle);

        if (!handle->connected) {
            // retry
            initHandshake(handle, msg);
        } else {
            delete msg;
        }
    } else {
        const char * const name = msg->getName();

        delete msg;

        throw cRuntimeError("Received unexpected message: %s", name);
    }
}

UDPTransport::SocketHandle_t* UDPTransport::createSocket() {
    SocketHandle_t * const handle = new SocketHandle_t();

    handle->socket.reset(new UDPSocket());
    handle->socket->setOutputGate(udpOut);

    return handle;
}

void UDPTransport::connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port) {
    ASSERT(handle);

    handle->socket->connect(endpAddr, port);

    handle->listening = false;
    handle->connected = false;
    handle->remote = endpAddr;
    handle->port = port;
}

void UDPTransport::storeSocket(SocketHandle_t * const handle) {
    ASSERT(handle);

    // may fail for incoming connections tied to the listen socket
    // thats ok, we use the map in first place to retrieve the listen socket or
    // to check if a socket context has been established. thus no assert here
    connectionMap.insert(SocketMap_t::value_type(handle->socket->getSocketId(), handle));

    connectionVect.insert(connectionVect.end(), handle);
}

void UDPTransport::initHandshake(SocketHandle_t * const handle, cMessage * const selfMsg) {
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
UDPTransport::SocketHandle_t * UDPTransport::getSocketById(const int id) {
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
UDPTransport::SocketHandle_t * UDPTransport::getSocketByAddr(const L3Address& addr, const uint16_t port) {
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
