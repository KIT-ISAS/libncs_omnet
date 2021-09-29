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

#include "OracleCCUDPTransport.h"


#include <NcsCpsApp.h>
#include <OracleCC/OracleCCSerumHeader_m.h>

#include <inet/common/RawPacket.h>
#include <inet/networklayer/diffserv/DSCP_m.h>

Define_Module(OracleCCUDPTransport);

#define TC_OCC_EF      DSCP_EF
#define TC_OCC_PRIO    DSCP_CS7
#define TC_OCC_LBE     DSCP_CS1

// randomly chosen ids
#define CONNECT_TICKER_KIND 9610
#define OCC_STREAM_START_EVT_MSG_KIND 9811
#define OCC_STREAM_STOP_EVT_MSG_KIND 9812

#define OCC_INACTIVITY_THRESHOLD 5


OracleCCUDPTransport::~OracleCCUDPTransport() {
    for (auto handle : connectionVect) {
        delete handle;
    }

    // no need to delete listenSocket, since it is also stored in connectionVect

    connectionMap.clear();
    connectionVect.clear();
}


OracleCCUDPTransport::SocketHandle_t::~SocketHandle_t() {
    if (streamStartEvent && !streamStartEvent->isScheduled()) {
        delete streamStartEvent;
    }
    if (streamStopEvent && !streamStopEvent->isScheduled()) {
        delete streamStopEvent;
    }
}

void OracleCCUDPTransport::initialize() {
    udpIn = gate("udpIn");
    udpOut = gate("udpOut");
    upIn = gate("up$i");
    upOut = gate("up$o");

    expectedRate = registerSignal("expectedRate");
    appliedQM = registerSignal("appliedQM");

    lowerLayerOverhead = par("lowerLayerOverhead").intValue();

    coexistenceMode = static_cast<CoCCUDPTransport::CoexistenceMode>(par("coexistenceMode").intValue());
    qmDesired = par("qmDesired").doubleValue();

    if (coexistenceMode < CoCCUDPTransport::CM_DISABLED || coexistenceMode > CoCCUDPTransport::CM_TOTAL_SUBMISSION) {
        error("unknown/unsupported coexistenceMode %d", coexistenceMode);
    }

    coord = OracleCCCoordinator::findCoordinator();

    ASSERT(coord);
}

void OracleCCUDPTransport::handleMessage(cMessage * const msg) {
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
                    TransportDataInfo * const info = createTransportInfo(ctrl);

                    msg->setControlInfo(info);

                    send(msg, upOut);
                } else {
                    EV_WARN << "No connection to " << ctrl->getSrcAddr() << " has been established yet. Dropping message: " << msg << endl;

                    delete msg;
                }
            } else {
                UDPHandshake * const hs = dynamic_cast<UDPHandshake *>(msg);

                ASSERT(hs);

                handleIncomingHandshake(hs, ctrl, handle);
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

            ASSERT(req);

            processConnectRequest(req);

            delete msg;
        } else if (dynamic_cast<TransportListenReq *>(ctrlInfo)) {
            const uint16_t listenPort = dynamic_cast<TransportListenReq *>(ctrlInfo)->getListenPort();

            delete msg;

            processListenRequest(listenPort);
        } else if (dynamic_cast<TransportSetTranslator *>(ctrlInfo)) {
            TransportSetTranslator * const req = dynamic_cast<TransportSetTranslator *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportSetTranslator control info in message.");
            }

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                if (req->getRole() == NCTXCI_CONTROLLER) { // OracleCC only cares about controller
                    occSetTranslator(handle, req->getTranslator());
                }
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportStreamStartInfo *>(ctrlInfo)) {
            TransportStreamStartInfo * const req = dynamic_cast<TransportStreamStartInfo *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportStreamStartInfo control info in message.");
            }

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                occHandleStreamStart(handle, req->getStart());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportStreamStopInfo *>(ctrlInfo)) {
            TransportStreamStopInfo * const req = dynamic_cast<TransportStreamStopInfo *>(ctrlInfo);

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportStreamStopInfo control info in message.");
            }

            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                occHandleStreamStop(handle, req->getStop());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
        } else if (dynamic_cast<TransportDataInfo *>(ctrlInfo)) {
            TransportDataInfo * const req = dynamic_cast<TransportDataInfo *>(msg->removeControlInfo());

            if (req == nullptr) {
                throw cRuntimeError("handleMessage(): expected TransportDataInfo control info in message.");
            }
            ASSERT(dynamic_cast<RawPacket *>(msg));

            RawPacket * const pkt = dynamic_cast<RawPacket *>(msg);
            SocketHandle_t * const handle = getSocketByAddr(req->getDstAddr(), req->getDstPort());

            if (handle) {
                ASSERT(req->getDstAddr() == handle->remote);

                if (handle->connected) {
                    handle->inactivityCounter = 0;

                    inet::UDPSocket::SendOptions opts;

                    opts.networkOptions = req->replaceNetworkOptions();

                    occCoexistenceHandler(handle, opts.networkOptions); // perform traffic differentiation, if enabled

                    handle->socket->sendTo(pkt, handle->remote, handle->port, &opts);
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
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received message with unknown control info type: %s", name);
        }
    } else if (msg->isSelfMessage()) {
        // Ticker event

        switch (msg->getKind()) {
        case OCC_STREAM_START_EVT_MSG_KIND:
            occHandleStreamStart(msg, simTime());
            break;
        case OCC_STREAM_STOP_EVT_MSG_KIND:
            occHandleStreamStop(msg, simTime());
            break;
        case CONNECT_TICKER_KIND:
            handleConnectTimeout(msg);
            break;
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

TransportDataInfo * OracleCCUDPTransport::createTransportInfo(UDPDataIndication * const ctrl) {
    TransportDataInfo * info = new TransportDataInfo();

    info->setSrcAddr(ctrl->getSrcAddr());
    info->setSrcPort(ctrl->getSrcPort());
    info->setDstAddr(ctrl->getDestAddr());
    info->setDstPort(ctrl->getDestPort());
    info->setNetworkOptions(ctrl->replaceNetworkOptions());

    return info;
}

void OracleCCUDPTransport::handleIncomingHandshake(UDPHandshake * const hs,
        UDPDataIndication * const ctrl, SocketHandle_t * const handle) {
    if (!hs->getSynAck()) {
        // listening part, create dedicated handle for sending packets from a listening host
        SocketHandle_t* const txHandle = new SocketHandle_t();

        txHandle->connected = true;
        txHandle->listening = false;
        txHandle->port = ctrl->getSrcPort();
        txHandle->remote = ctrl->getSrcAddr();
        txHandle->socket = handle->socket; // re-use listen-socket

        storeSocket(txHandle);

        // finish path recording
        auto opts = ctrl->getNetworkOptions();

        if (opts) {
            const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

            if (index >= 0) {
                auto eh = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(index));

                ASSERT(eh);

                auto inbound = SerumSupport::extractPush(eh, DATASET_OCC_DISCOVER_INBOUND);

                if (inbound.size() > 0) {
                    ASSERT(inbound.size() == 1);

                    auto req = dynamic_cast<OracleCCDiscoverRecord *>(inbound[0]);

                    OracleCCCoordinator::endPath(req->getSrc(), req->getHandle());
                }

                // do not return the header header with ACK
                opts->removeV6Header(index);

                delete eh;
            }
        }

        // send ACK
        hs->setSynAck(true);
        txHandle->socket->sendTo(hs, txHandle->remote, txHandle->port);

    } else {
        // client side part, set port and flag connection as established
        handle->port = ctrl->getSrcPort();
        handle->connected = true;

        delete hs;

        // confirm connection to upper layer
        auto conf = new TransportConnectReq();
        auto msg = new cMessage("Connection confirmation", CpsConnReq);

        conf->setDstAddr(handle->remote);
        conf->setDstPort(handle->port);
        msg->setControlInfo(conf);

        send(msg, upOut);
    }
}

void OracleCCUDPTransport::processConnectRequest(TransportConnectReq* const req) {
    SocketHandle_t* const handle = createSocket();

    connectSocket(handle, req->getDstAddr(), req->getDstPort());
    storeSocket(handle);

    // timeout notification for retries
    cMessage* const selfMsg = new cMessage("Connect Timeout", CONNECT_TICKER_KIND);

    selfMsg->setContextPointer(handle);

    // send connect notification to server and start timeout
    initHandshake(handle, selfMsg);
}

void OracleCCUDPTransport::processListenRequest(const uint16_t listenPort) {
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
}

void OracleCCUDPTransport::handleConnectTimeout(cMessage* const msg) {
    // Timeout-Ticker
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    if (!handle->connected) {
        // retry
        initHandshake(handle, msg);
    } else {
        delete msg;
    }
}

void OracleCCUDPTransport::occHandleStreamStart(cMessage * const msg, simtime_t start) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    occHandleStreamStart(handle, start);
}

void OracleCCUDPTransport::occHandleStreamStart(SocketHandle_t * const handle, simtime_t start) {
    if (start > simTime()) {
        if (handle->streamStartEvent) {
            if (handle->streamStartEvent->isScheduled()) {
                cancelEvent(handle->streamStartEvent);
            }
        } else {
           handle->streamStartEvent = new cMessage("OCC stream start event", OCC_STREAM_START_EVT_MSG_KIND);
           handle->streamStartEvent->setContextPointer(handle);
        }

        handle->translator->setTargetQM(0);

        // schedule for later start
        scheduleAt(start, handle->streamStartEvent);
    } else {
        handle->inactivityCounter = 0;
    }
}

void OracleCCUDPTransport::occHandleStreamStop(cMessage * const msg, simtime_t stop) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    occHandleStreamStop(handle, stop);
}

void OracleCCUDPTransport::occHandleStreamStop(SocketHandle_t * const handle, simtime_t stop) {
    EV_DEBUG << "Received stream stop signal, ticker will time out after this period." << endl;

    if (stop > simTime()) {
        if (handle->streamStopEvent) {
            if (handle->streamStopEvent->isScheduled()) {
                cancelEvent(handle->streamStopEvent);
            }
        } else {
           handle->streamStopEvent = new cMessage("OCC stream stop event", OCC_STREAM_STOP_EVT_MSG_KIND);
           handle->streamStopEvent->setContextPointer(handle);
        }

        handle->translator->setTargetQM(0);
        emit(appliedQM, 0.0);

        // schedule for later stop
        scheduleAt(stop, handle->streamStopEvent);
    } else {
        handle->inactivityCounter = OCC_INACTIVITY_THRESHOLD;

        // reset to QM 0
        postControlStep(handle);
    }

}

void OracleCCUDPTransport::occSetTranslator(SocketHandle_t * const handle, ICoCCTranslator* const translator) {
    auto oldPtr = handle->translator;

    handle->translator = translator;

    // set overhead values to allow for more precise calculations in translator
    translator->setPerPacketOverhead(lowerLayerOverhead);
    translator->setNetworkOverhead(0);

    // register to observe control events
    translator->setControlObserver(this, handle);

    if (oldPtr != nullptr) {
        oldPtr->setControlObserver(nullptr); // unregister
    }
}

void OracleCCUDPTransport::occCoexistenceHandler(SocketHandle_t * const handle, NetworkOptions* &opts) {
    if (handle->translator) { // nothing to do for non-controller devices
        if (!opts) {
            opts = new NetworkOptions();
        }

        // ACKs and metadata push/request packets are already flagged as EF, do not change
        int tc = opts->getTrafficClass();

        // flag all packets which are not default BE class as high priority
        tc = (tc != TC_OCC_EF) ? TC_OCC_PRIO : tc;

        switch (coexistenceMode) {
        case CoCCUDPTransport::CM_REACTIVE: {
            // only in reactive mode: divide flows between TC_COCC_PRIO and TC_COCC_LBE
            const double actualQM = handle->translator->getActualQM();
            const double targetQM = handle->translator->getTargetQM();
            const double targetQMRate = handle->translator->getRateForQM(targetQM, targetQM);
            const double desiredQMRate = handle->translator->getRateForQM(actualQM, qmDesired);
            // fraction of targetQMRate which is required to have better QM than qmDesired
            const double lbeFraction = CLAMP((targetQMRate - desiredQMRate) / targetQMRate, 0.0, 1.0);

            handle->lbeClassAccumulator += lbeFraction;

            if (tc != TC_OCC_EF && handle->lbeClassAccumulator >= 1) {
                tc = TC_OCC_LBE;
                handle->lbeClassAccumulator -= 1;
            }
            } break;
        case CoCCUDPTransport::CM_SUBMISSION: {
            if (tc != TC_OCC_EF) {
                tc = TC_OCC_LBE;
            }
            } break;
        case CoCCUDPTransport::CM_TOTAL_SUBMISSION: {
            tc = TC_OCC_LBE;
            } break;
        default:
            break;
        }

        if (tc == TC_OCC_EF) {
            tc = TC_OCC_PRIO; // EF forwarding is not required since we do not collect any data from the network
        }

        opts->setTrafficClass(tc);
    }
}

void OracleCCUDPTransport::postControlStep(void * const context) {
    SocketHandle_t * const handle = (SocketHandle_t *)context;

    ASSERT(handle);

    double qm;
    double qmRate;

    if (handle->inactivityCounter < OCC_INACTIVITY_THRESHOLD) {
        qm = coord->getFlowTargetQM(this, handle);
        qmRate = handle->translator->getRateForQM(handle->translator->getActualQM(), qm);
    } else {
        qm = 0;
        qmRate = 0;
    }

    if (handle->translator->getTargetQM() != qm) { // reduce update/reporting frequency
        handle->translator->setTargetQM(qm);

        emit(appliedQM, qm);
        emit(expectedRate, qmRate);
    }
}

double OracleCCUDPTransport::getQMDesiredRate(void * const handle) {
    SocketHandle_t * const h = (SocketHandle_t *)handle;

    ASSERT(handle);

    if (h->translator && h->inactivityCounter < OCC_INACTIVITY_THRESHOLD)  {
        return h->translator->getRateForQM(h->translator->getActualQM(), qmDesired);
    } else {
        return 0;
    }
}

double OracleCCUDPTransport::getRateForQM(void * const handle, const double qmTarget) {
    SocketHandle_t * const h = (SocketHandle_t *)handle;

    ASSERT(handle);

    if (h->translator && h->inactivityCounter < OCC_INACTIVITY_THRESHOLD)  {
        return h->translator->getRateForQM(h->translator->getActualQM(), qmTarget);
    } else {
        return 0;
    }
}

ICoCCTranslator::CoCCLinearization OracleCCUDPTransport::getLinearizationForQM(void * const handle, const double qmTarget) {
    SocketHandle_t * const h = (SocketHandle_t *)handle;

    ASSERT(handle);

    if (h->translator && h->inactivityCounter < OCC_INACTIVITY_THRESHOLD)  {
        return h->translator->getLinearizationForRate(h->translator->getActualQM(), qmTarget);
    } else {
        return { 0, 0 };
    }
}

OracleCCUDPTransport::SocketHandle_t * OracleCCUDPTransport::createSocket() {
    SocketHandle_t * const handle = new SocketHandle_t();

    handle->socket.reset(new UDPSocket());
    handle->socket->setOutputGate(udpOut);

    handle->inactivityCounter = OCC_INACTIVITY_THRESHOLD;

    return handle;
}

void OracleCCUDPTransport::connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port) {
    ASSERT(handle);

    handle->socket->connect(endpAddr, port);

    handle->listening = false;
    handle->connected = false;
    handle->remote = endpAddr;
    handle->port = port;
}

void OracleCCUDPTransport::storeSocket(SocketHandle_t * const handle) {
    ASSERT(handle);

    // may fail for incoming connections tied to the listen socket
    // thats ok, we use the map in first place to retrieve the listen socket or
    // to check if a socket context has been established. thus no assert here
    connectionMap.insert(SocketMap_t::value_type(handle->socket->getSocketId(), handle));

    connectionVect.insert(connectionVect.end(), handle);
}

void OracleCCUDPTransport::initHandshake(SocketHandle_t * const handle, cMessage * const selfMsg) {
    // send connect notification to server
    UDPHandshake* const hs = new UDPHandshake("Connect Handshake");

    hs->setSynAck(false);

    // add SERUM push for inbound and outbound
    inet::UDPSocket::SendOptions opts;
    opts.networkOptions = new NetworkOptions();

    OracleCCDiscoverRecord * const inbound = new OracleCCDiscoverRecord();
    OracleCCDiscoverRecord * const outbound = new OracleCCDiscoverRecord();

    inbound->setDataDesc(DATASET_OCC_DISCOVER_INBOUND);
    outbound->setDataDesc(DATASET_OCC_DISCOVER_OUTBOUND);
    inbound->setSrc(this);
    outbound->setSrc(this);
    inbound->setHandle(handle);
    outbound->setHandle(handle);

    {
        IPv6HopByHopOptionsHeader * doh = nullptr;

        const short index = opts.networkOptions->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

        if (index >= 0) {
            auto doh = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts.networkOptions->getV6Header(index));
            ASSERT(doh);
        } else {
            doh = new IPv6HopByHopOptionsHeader();

            opts.networkOptions->addV6Header(doh);
        }

        doh->getTlvOptions().add(inbound);
        doh->getTlvOptions().add(outbound);
    }

    handle->socket->sendTo(hs, handle->remote, handle->port, &opts);

    scheduleAt(simTime() + 0.25, selfMsg);
}

/**
 * Returns either the listen socket which might be used for several incoming
 * connections, or the socket associated with an outgoing connection.
 */
OracleCCUDPTransport::SocketHandle_t * OracleCCUDPTransport::getSocketById(const int id) {
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
OracleCCUDPTransport::SocketHandle_t * OracleCCUDPTransport::getSocketByAddr(const L3Address& addr, const uint16_t port) {
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
