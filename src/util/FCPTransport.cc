#include "FCPTransport.h"

Define_Module(FCPTransport);

FCPTransport::FCPTransport() {

}

FCPTransport::~FCPTransport() {
    for (auto it = connectionMap.begin(); it != connectionMap.end();){
        delete it->second;

        it = connectionMap.erase(it);
    }
}

void FCPTransport::initialize(){
    fcpIn = gate("fcpIn");
    fcpOut = gate("fcpOut");
    upIn = gate("up$i");
    upOut = gate("up$o");

    function = par("function").intValue();
    percentage = par("percentage").doubleValue();
    usePreload = par("usePreload").boolValue();

    enableQMSmoothing = par("enableQMSmoothing").boolValue();
    qmHistoryLength = par("qmHistoryLength").intValue();

    collectionInterval = par("collectionInterval").doubleValue();
    forcedPushDelay = par("forcedPushDelay").doubleValue();
    fcpEpsilon = par("fcpEpsilon").doubleValue();
    fcpQ = par("fcpQ").doubleValue();

    metadataOverhead = par("metadataOverhead").intValue();
    lowerLayerOverhead = par("lowerLayerOverhead").intValue();
    fcpOverhead = par("fcpOverhead").intValue();
}

void FCPTransport::handleMessage(cMessage * msg){
    if (msg->arrivedOn(fcpIn->getId())){
        FCPCommand * const indication = dynamic_cast<FCPCommand *>(msg->getControlInfo());

        int connId = indication->getConnId();
        FCPSocket* socket = getSocketById(connId);

        switch(msg->getKind()){
        case FCP_I_ESTABLISHED: {
            EV_DETAIL << "Got a message from FCP and updated the socket." << endl;

            socket->processMessage(msg);

            delete msg;
            break;
        }
        case FCP_I_DATA: {
            msg->removeControlInfo();
            delete indication;

            TransportDataInfo * const info = createTransportInfo(socket);

            msg->setControlInfo(info);

            send(msg, upOut);

            EV_DETAIL << "Got a message from FCP and forwarded it to the application" << endl;
            break;
        }
        default:
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received unexpected message: %s", name);
        }
    } else if(msg->arrivedOn(upIn->getId())){
        cObject * const ctrlInfo = msg->getControlInfo();

        if (dynamic_cast<TransportConnectReq *>(ctrlInfo)){
            TransportConnectReq * const req = dynamic_cast<TransportConnectReq *>(ctrlInfo);
            FCPSocket * socket = createSocket();

            connectSocket(socket, req->getDstAddr(), req->getDstPort());
            storeSocket(socket);

            delete msg;

        } else if (dynamic_cast<TransportListenReq *>(ctrlInfo)){
            uint16_t listenPort = dynamic_cast<TransportListenReq *>(ctrlInfo)->getListenPort();
            FCPSocket * socket = createSocket();

            socket->bind(listenPort);
            socket->listen();
            storeSocket(socket);

            delete msg;

        } else if (dynamic_cast<TransportDataInfo *>(ctrlInfo)){
            TransportDataInfo * const req = dynamic_cast<TransportDataInfo *>(msg->removeControlInfo());
            FCPSocket * socket = getSocketByAddr(req->getDstAddr());

            if(socket){
                socket->send(msg);
            } else{
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;

                delete msg;
            }
            delete req;
        } else if (dynamic_cast<TransportStreamStartInfo *>(ctrlInfo)) {
            delete msg; // ignore
        } else if (dynamic_cast<TransportStreamStopInfo *>(ctrlInfo)) {
            delete msg; // ignore
        } else if (dynamic_cast<TransportSetTranslator *>(ctrlInfo)) {
            TransportSetTranslator * const req = dynamic_cast<TransportSetTranslator *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportSetTranslator control info in message.");
            }

            FCPSocket * handle = getSocketByAddr(req->getDstAddr());

            if (handle) {
                handle->setTranslator(req->getTranslator());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportStreamStartInfo *>(ctrlInfo)) {
            TransportStreamStartInfo * const req = dynamic_cast<TransportStreamStartInfo *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportStreamStartInfo control info in message.");
            }

            FCPSocket * const handle = getSocketByAddr(req->getDstAddr());

            if (handle) {
                handle->setTransportStreamStart(req->getStart());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;

        } else {
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received message with unknown control info type: %s", name);
        }
    } else{
        const char * const name = msg->getName();

        delete msg;

        throw cRuntimeError("Received unexpected message: %s", name);
    }
}

TransportDataInfo* FCPTransport::createTransportInfo(FCPSocket* socket){
    TransportDataInfo* info = new TransportDataInfo();
    info->setSrcAddr(socket->getRemoteAddress());
    info->setSrcPort(socket->getRemotePort());
    info->setDstAddr(socket->getLocalAddress());
    info->setDstPort(socket->getLocalPort());
    return info;
}

FCPSocket* FCPTransport::createSocket(){
    FCPSocket * socket = new FCPSocket(function, percentage, usePreload, collectionInterval, forcedPushDelay, fcpEpsilon, fcpQ, metadataOverhead,
            lowerLayerOverhead, fcpOverhead, enableQMSmoothing, qmHistoryLength);
    socket->setOutputGate(fcpOut);
    return socket;
}

void FCPTransport::connectSocket(FCPSocket * handle, L3Address endpAddr, uint16_t port){
    handle->connect(endpAddr, port);
}

void FCPTransport::storeSocket(FCPSocket * handle){
    connectionMap.insert(SocketMap_t::value_type(handle->getConnectionId(), handle));
}

FCPSocket * FCPTransport::getSocketById(int id){
    auto iterator = connectionMap.find(id);
    if (iterator == connectionMap.end()) {
        return nullptr;
    }
    return iterator->second;
}

FCPSocket * FCPTransport::getSocketByAddr(L3Address& addr){
    for (auto iterator = connectionMap.begin(); iterator != connectionMap.end(); ++iterator) {
        if (iterator->second->getRemoteAddress() == addr) {
            return iterator->second;
        }
    }
    return nullptr;
}

void FCPTransport::deleteSocket(FCPSocket * handle){
    auto iterator = connectionMap.find(handle->getConnectionId());
    connectionMap.erase(iterator);
    delete handle;
}
