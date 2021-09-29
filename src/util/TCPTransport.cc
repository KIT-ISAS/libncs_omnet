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

#include "TCPTransport.h"

#include <NcsCpsApp.h>

#include <inet/common/RawPacket.h>

Define_Module(TCPTransport);

TCPTransport::TCPTransport() {

}

TCPTransport::~TCPTransport() {
    for (auto it = connectionMap.begin(); it != connectionMap.end();) {
        delete it->second;

        it = connectionMap.erase(it);
    }
}

void TCPTransport::initialize() {
    tcpIn = gate("tcpIn");
    tcpOut = gate("tcpOut");
    upIn = gate("up$i");
    upOut = gate("up$o");

    bytestreamService = par("bytestreamService");
    datagramService = par("datagramService");
}

void TCPTransport::handleMessage(cMessage * const msg) {
    if (msg->arrivedOn(tcpIn->getId())) {
        TCPCommand * const ind = dynamic_cast<TCPCommand *>(msg->getControlInfo());

        if (!ind) {
            throw cRuntimeError("handleMessage(): no TCPCommand control info in message (not from TCP?)");
        }

        const int connId = ind->getConnId();

        SocketHandle_t * handle = getSocketById(connId);

        if (!handle) {
            if (!listening) {
                throw cRuntimeError(
                        "getSocketById(): no TCP socket matching the connectionId found.");
            }

            EV << getFullPath() << " got new incoming connection with id " << connId << endl;

            handle = createSocket(msg);
            storeSocket(handle);
        }

        ASSERT(handle);

        handle->socket.processMessage(msg);
    } else if (msg->arrivedOn(upIn->getId())) {
        cObject * const ctrlInfo = msg->getControlInfo();

        if (dynamic_cast<TransportConnectReq *>(ctrlInfo)) {
            TransportConnectReq * const req = dynamic_cast<TransportConnectReq *>(ctrlInfo);
            SocketHandle_t * const handle = createSocket();

            connectSocket(handle, req->getDstAddr(), req->getDstPort());
            storeSocket(handle);

            EV << getFullPath() << " is connecting to " << req->getDstAddr().str() << ":" << req->getDstPort() << " << connId: " << handle->socket.getConnectionId() << endl;

            delete msg;
        } else if (dynamic_cast<TransportListenReq *>(ctrlInfo)) {
            const uint16_t listenPort = dynamic_cast<TransportListenReq *>(ctrlInfo)->getListenPort();

            delete msg;

            if (!listening) {
                SocketHandle_t * const handle = createSocket();

                handle->socket.bind(listenPort);
                handle->socket.listen();

                storeSocket(handle);

                listening = true;

                EV << getFullPath() << " is listening at Port " << listenPort << endl;
            } else {
                throw cRuntimeError("Already listening, refusing to listen twice");
            }
        } else if (dynamic_cast<TransportDataInfo *>(ctrlInfo)) {
            TransportDataInfo * const req = dynamic_cast<TransportDataInfo *>(msg->removeControlInfo());
            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr());

            if (handle) {
                if (datagramService && bytestreamService) {
                    RawPacket * const rawPkt = dynamic_cast<RawPacket *>(msg);

                    ASSERT(rawPkt);
                    // add leading header (payloadSize)
                    const PayloadSize_t payloadSize = rawPkt->getByteArray().getDataArraySize();

                    ASSERT(rawPkt->getByteArray().getDataArraySize() + sizeof(PayloadSize_t) < static_cast<PayloadSize_t>(-1));

                    rawPkt->getByteArray().expandData(sizeof(PayloadSize_t), 0);
                    rawPkt->getByteArray().copyDataFromBuffer(0, &payloadSize, sizeof(PayloadSize_t));
                    rawPkt->setByteLength(payloadSize + sizeof(PayloadSize_t));
                }

                handle->socket.send(msg);
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;

                delete msg;
            }

            delete req;
        } else {
            EV_WARN << "Received message with unknown control info type, ignoring: " << msg << endl;

            delete msg;
        }
    } else {
        const char * const name = msg->getName();

        delete msg;

        throw cRuntimeError("Received unexpected message: %s", name);
    }
}

void TCPTransport::socketDataArrived(const int connId, void * const yourPtr, cPacket * const msg, const bool urgent) {
    SocketHandle_t * const handle = static_cast<SocketHandle_t *>(yourPtr);

    if (datagramService && bytestreamService) {
        RawPacket * const rawPkt = dynamic_cast<RawPacket *>(msg);

        ASSERT(rawPkt);

        // reassemble Packet
        ASSERT(rawPkt->getByteLength() <= rawPkt->getByteArray().getDataArraySize());

        // copy new Packet into buffer
        handle->buffer.addDataFromBuffer(rawPkt->getByteArray().getDataPtr(), rawPkt->getByteLength());

        delete rawPkt; // msg is invalid now, too

        while (handle->buffer.getDataArraySize() > sizeof(PayloadSize_t)) { // may start to parse
            PayloadSize_t * const payloadSize = reinterpret_cast<PayloadSize_t *>(handle->buffer.getDataPtr());

            if (handle->buffer.getDataArraySize() >= *payloadSize + sizeof(PayloadSize_t)) { // message completed
                RawPacket * const outPkt = new RawPacket("TCPTransport RAW Payload");

                outPkt->setControlInfo(createDataInfo(handle));
                outPkt->getByteArray().setDataFromByteArray(handle->buffer, sizeof(PayloadSize_t), *payloadSize);
                outPkt->setByteLength(*payloadSize);

                send(outPkt, upOut);

                // strip packet from buffer
                handle->buffer.truncateData(*payloadSize + sizeof(PayloadSize_t), 0);
            } else {
                break;
            }
        }
    } else {
        delete msg->removeControlInfo();

        msg->setControlInfo(createDataInfo(handle));

        send(msg, upOut);
    }
}

TransportDataInfo * TCPTransport::createDataInfo(SocketHandle_t* const handle) {
    TransportDataInfo * const info = new TransportDataInfo();

    info->setSrcAddr(handle->socket.getRemoteAddress());
    info->setSrcPort(handle->socket.getRemotePort());
    info->setDstAddr(handle->socket.getLocalAddress());
    info->setDstPort(handle->socket.getLocalPort());

    return info;
}

void TCPTransport::socketEstablished(const int connId, void *const yourPtr) {
    SocketHandle_t * const handle = static_cast<SocketHandle_t *>(yourPtr);

    if (!handle) {
        throw cRuntimeError("socketEstablished(): Got unexpected yourPtr");
    }

    // confirm connection to upper layer
    auto conf = new TransportConnectReq();
    auto msg = new cMessage("Connection confirmation", CpsConnReq);

    conf->setDstAddr(handle->socket.getRemoteAddress());
    conf->setDstPort(handle->socket.getRemotePort());
    msg->setControlInfo(conf);

    send(msg, upOut);

    EV << "socketEstablished(): " << connId << std::endl;
}

void TCPTransport::socketPeerClosed(const int connId, void * const yourPtr) {
    SocketHandle_t * const handle = static_cast<SocketHandle_t *>(yourPtr);

    if (!handle) {
        throw cRuntimeError("socketPeerClosed(): Got unexpected yourPtr");
    }

    deleteSocket(handle);

    EV << "socketPeerClosed():" << connId << std::endl;
}

void TCPTransport::socketClosed(const int connId, void * const yourPtr) {
    SocketHandle_t * const handle = static_cast<SocketHandle_t *>(yourPtr);

    if (!handle) {
        throw cRuntimeError("socketPeerClosed(): Got unexpected yourPtr");
    }

    deleteSocket(handle);

    EV << "socketClosed():" << connId << std::endl;
}

void TCPTransport::socketFailure(const int connId, void * const yourPtr, const int code) {
    SocketHandle_t * const handle = static_cast<SocketHandle_t *>(yourPtr);

    if (!handle) {
        throw cRuntimeError("socketPeerClosed(): Got unexpected yourPtr");
    }

    deleteSocket(handle);

    EV << "socketFailure():" << connId << std::endl;
}

void TCPTransport::socketStatusArrived(const int connId, void *const yourPtr, TCPStatusInfo * const status) {
    EV << "socketStatusArrived():" << connId << std::endl;

    delete status;
}

void TCPTransport::socketDeleted(const int connId, void * const yourPtr) {
    EV << "socketDeleted():" << connId << std::endl;
}

TCPTransport::SocketHandle_t* TCPTransport::createSocket(cMessage * const msg) {
    SocketHandle_t * const handle = new SocketHandle_t();

    if (msg != nullptr) {
        handle->socket = TCPSocket(msg);
    }

    handle->socket.setOutputGate(tcpOut);
    handle->socket.setCallbackObject(this, handle);
    handle->socket.setDataTransferMode(bytestreamService ? TCP_TRANSFER_BYTESTREAM : TCP_TRANSFER_OBJECT);

    return handle;
}

void TCPTransport::connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port) {
    ASSERT(handle);

    handle->socket.connect(endpAddr, port);
    // One could configure the congestion control algorithm and other parameters here
}

void TCPTransport::storeSocket(SocketHandle_t * const handle) {
    ASSERT(handle);

    auto mapResult = connectionMap.insert(SocketMap_t::value_type(handle->socket.getConnectionId(), handle));

    ASSERT(mapResult.second);
}

TCPTransport::SocketHandle_t * TCPTransport::getSocketById(const int id) {
    const auto it = connectionMap.find(id);

    if (it == connectionMap.end()) {
        return nullptr;
    }

    SocketHandle_t * const handle = it->second;
    ASSERT(it->first == handle->socket.getConnectionId());

    return handle;
}

TCPTransport::SocketHandle_t * TCPTransport::getSocketByAddr(const L3Address& addr) {
    SocketHandle_t * handle = nullptr;

    // Maybe later replace by a Map
    for (auto it = connectionMap.begin(); it != connectionMap.end(); ++it) {
        if (it->second->socket.getRemoteAddress() == addr) {
            handle = it->second;
        }
    }

    return handle;
}

void TCPTransport::deleteSocket(SocketHandle_t * const handle) {
    ASSERT(handle);

    const auto connIt = connectionMap.find(handle->socket.getConnectionId());

    if (connIt == connectionMap.end()) {
        throw cRuntimeError(
                "dropSocket(): no matching TCP socket handle found.");
    }

    connectionMap.erase(connIt);

    delete handle;
}
