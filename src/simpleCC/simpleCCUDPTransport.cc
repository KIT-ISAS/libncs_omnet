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


#include <NcsCpsApp.h>
#include <simpleCC/simpleCCUDPTransport.h>
#include "simpleCCMsg_m.h"
#include <algorithm>
#include <inet/common/RawPacket.h>
#include <inet/networklayer/diffserv/DSCP_m.h>

Define_Module(simpleCCUDPTransport);

#define TC_simpleCC DSCP_EF
// randomly chosen ids
#define CONNECT_TICKER_KIND 9810
#define simpleCC_PUSH_TICKER_MSG_KIND 9811
#define simpleCC_EMPTY_PUSH_MSG_KIND 9821

#define simpleCC_INACTIVITY_THRESHOLD 5




simpleCCUDPTransport::simpleCCUDPTransport() {

}

simpleCCUDPTransport::~simpleCCUDPTransport() {
    for (auto handle : connectionVect) {
        delete handle;
    }

    // no need to delete listenSocket, since it is also stored in connectionVect

    connectionMap.clear();
    connectionVect.clear();
}

simpleCCUDPTransport::SocketHandle_t::~SocketHandle_t() {
    if (pushTicker && !pushTicker->isScheduled()) {
        delete pushTicker;
    }
}

void simpleCCUDPTransport::initialize() {
    udpIn = gate("udpIn");
    udpOut = gate("udpOut");
    upIn = gate("up$i");
    upOut = gate("up$o");
    //above standard, below new stuff, necessary?
    expectedRate = registerSignal("expectedRate");

    forceMonitoringReply = par("forceMonitoringReply").boolValue();

    collectionInterval = par("collectionInterval").doubleValue();
    pushSpread = par("pushSpread").doubleValue();
    forcedPushDelay = par("forcedPushDelay").doubleValue();
    forcedPushSpread = par("forcedPushSpread").doubleValue();

    lowerLayerOverhead = par("lowerLayerOverhead").intValue();
    metadataOverhead = par("metadataOverhead").intValue();
    // new simple CC
    simpleCC_QUEUE_THRESHOLD = par("simpleCC_QUEUE_THRESHOLD").doubleValue();
    simpleCC_FULL_LOAD_THRESHOLD = par("simpleCC_FULL_LOAD_THRESHOLD").doubleValue();
    simpleCC_HEAVY_CONGESTION_MULT = par("simpleCC_HEAVY_CONGESTION_MULT").doubleValue();
    simpleCC_LIGHT_CONGESTION_MULT = par("simpleCC_LIGHT_CONGESTION_MULT").doubleValue();
    simpleCC_SLOW_START_THRESHOLD_MULT = par("simpleCC_SLOW_START_THRESHOLD_MULT").doubleValue();
    simpleCC_QM_STEP = par("simpleCC_QM_STEP").doubleValue();
    simpleCC_CONGESTION_THRESHOLD = par("simpleCC_CONGESTION_THRESHOLD").doubleValue();
    simpleCC_enable_random_fullLoadIncrease = par("simpleCC_enable_random_fullLoadIncrease").boolValue();
    simpleCC_enable_dynamic_maxDrop = par("simpleCC_enable_dynamic_maxDrop").boolValue();
    simpleCC_enable_weigthed_slowStartTH = par("simpleCC_enable_weigthed_slowStartTH").boolValue();
    simpleCC_RANDOM_MIN_QUEUE_THRESHOLD = par("simpleCC_RANDOM_MIN_QUEUE_THRESHOLD").doubleValue();
    simpleCC_RANDOM_MAX_QUEUE_THRESHOLD = par("simpleCC_RANDOM_MAX_QUEUE_THRESHOLD").doubleValue();
    simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING = par("simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING").doubleValue();

    simpleCC_c = par("simpleCC_c").doubleValue();
    simpleCC_beta = par("simpleCC_beta").doubleValue();

}

void simpleCCUDPTransport::handleMessage(cMessage * const msg) {
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
                    simpleCCProcessMonitoringRequest(listenHandle, info);

                    simpleCCProcessFeedbackCubic(listenHandle, info);

                    msg->setControlInfo(info);

                    if (!dynamic_cast<simpleCCMetadataPushPkt *>(msg)) {
                        send(msg, upOut);
                    } else {
                        delete msg; // msg is just an empty push message to forward metadata, can be dropped
                    }
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

                simpleCCSetTranslator(handle, req->getTranslator());
            } else {
                EV_WARN << "unable to find socket for destAddr " << req->getDstAddr() << " dropping message: " << msg << endl;
            }

            delete msg;
            } else if (dynamic_cast<TransportStreamStartInfo *>(ctrlInfo)) {

            // unused

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

                    simpleCCReplyToMonitoringRequest(handle, opts.networkOptions); // reply to monitoring request, if pending
                    simpleCCPushMetadata(handle, opts.networkOptions); // initiate push if not already done in period


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

        switch (msg->getKind()) {
        case simpleCC_PUSH_TICKER_MSG_KIND:

            simpleCCHandlePushTicker(msg);

            break;
        case CONNECT_TICKER_KIND:
            // standard udp case
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

TransportDataInfo * simpleCCUDPTransport::createTransportInfo(UDPDataIndication * const ctrl) {
    TransportDataInfo * info = new TransportDataInfo();

    info->setSrcAddr(ctrl->getSrcAddr());
    info->setSrcPort(ctrl->getSrcPort());
    info->setDstAddr(ctrl->getDestAddr());
    info->setDstPort(ctrl->getDestPort());

    info->setNetworkOptions(ctrl->replaceNetworkOptions());

    return info;
}

void simpleCCUDPTransport::handleIncomingHandshake(UDPHandshake * const hs,
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

void simpleCCUDPTransport::processConnectRequest(TransportConnectReq* const req) {
    SocketHandle_t* const handle = createSocket();

    connectSocket(handle, req->getDstAddr(), req->getDstPort());
    storeSocket(handle);

    // timeout notification for retries
    cMessage* const selfMsg = new cMessage("Connect Timeout", CONNECT_TICKER_KIND);

    selfMsg->setContextPointer(handle);

    // send connect notification to server and start timeout
    initHandshake(handle, selfMsg);
}

void simpleCCUDPTransport::processListenRequest(const uint16_t listenPort) {
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

void simpleCCUDPTransport::handleConnectTimeout(cMessage* const msg) {
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

void simpleCCUDPTransport::simpleCCProcessMonitoringRequest(SocketHandle_t * const handle, TransportDataInfo * const info) {
    ASSERT(handle);
    ASSERT(info);

    NetworkOptions * const opts = info->getNetworkOptions();

    if (!opts) {
        return;
    }

    const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_DEST);

    if (index >= 0) {
        auto eh = dynamic_cast<IPv6DestinationOptionsHeader *>(opts->getV6Header(index));

        ASSERT(eh);

        if (SerumSupport::containsRequest(eh, DATASET_simpleCC_RESP)) {
            handle->pendingRequest.reset(eh->dup());

            // kinda a quirk to provide monitoring data if no ACKs are sent back from higher layer
            if (forceMonitoringReply && handle->connected) {
                inet::UDPSocket::SendOptions opts;

                simpleCCReplyToMonitoringRequest(handle, opts.networkOptions);

                cPacket * const pkt = new simpleCCMetadataPushPkt("simpleCC monitoring reply packet", simpleCC_EMPTY_PUSH_MSG_KIND);

                handle->socket->sendTo(pkt, handle->remote, handle->port, &opts);
                EV_DEBUG << "ProcessMonitoring Request pkt: " << pkt << endl;
            }
        }
    }

}


void simpleCCUDPTransport::simpleCCProcessFeedbackCubic(SocketHandle_t * const handle, TransportDataInfo * const info) {

    ASSERT(handle);
        ASSERT(info);


        NetworkOptions * const opts = info->getNetworkOptions();
        if (!handle->translator || !opts) {
            return;
        }

        const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

        if (index < 0) {
            return;
        }

        auto hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(index));

        ASSERT(hho);

        if (!SerumSupport::containsResponse(hho, DATASET_simpleCC_RESP)) {
            return;
        }

        // got simpleCC feedback, compute new QMTarget

        const std::vector<SerumRecord *> records = SerumSupport::extractResponse(hho, DATASET_simpleCC_RESP);
        double slowStartTH = slowStartThreshold;

        // set min slowStartTh for this cycle
        for (auto record : records){
            simpleCCResponseRecord * const r = dynamic_cast<simpleCCResponseRecord *>(record);
            ASSERT(r);
            //XXX: this is nonsense, does nothing but burn cycles
            slowStartTH = std::min(slowStartTH, slowStartThreshold);
        }
        EV_DEBUG << "slowStartTHMinimum set as  = " << slowStartTH << endl;



        bool oldCongestionBlocked = congestionBlocked;
        double qm_mult = 0;
        double newQM = 0;
        double targetQM =  handle->translator->getTargetQM();
        double newTargetQM = targetQM; // variable, to store the next targetQm. Can be changed several times during a turn
        double randomQueueThreshold =
                simpleCC_QUEUE_THRESHOLD * uniform(simpleCC_RANDOM_MIN_QUEUE_THRESHOLD, simpleCC_RANDOM_MAX_QUEUE_THRESHOLD); // desync of flow feedback, but causes flows to span out


        // scale randomQueueThreshold dynamicly with targetQM
        if(simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING >= 0){
            randomQueueThreshold = 1.0 / (1 + simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING * targetQM) * randomQueueThreshold;

        }
        else{
            EV_ERROR << "negative simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING :  " <<  simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING << endl;
            return;
        }

        EV_DEBUG << "randomQueueThreshold :  " <<  randomQueueThreshold << endl;
        EV_DEBUG << "old Target QM :  " <<  targetQM << endl;

        double time = elapsedCycles/10;
        double k = std::cbrt(simpleCC_beta/simpleCC_c * lastCongestionQm);
        EV_DEBUG << "congestion k :  " <<  k << endl;





        // slowStart
        if(targetQM < slowStartTH/2){
            time = elapsedCycles/10;
            EV_DEBUG << "slowStartTime lower half:  " <<  time << endl;
            // cubic function without the k -factor to gain the latter half of it
            maxTqmStep = simpleCC_c * pow(simpleCC_c * time, 3.0);
            EV_DEBUG << "slowStartStep lower half:  " <<  maxTqmStep << endl;
        }
        else{
            if(targetQM < slowStartTH){
                maxTqmStep = slowStartTH/10;

            }
            else{
                EV_DEBUG << "slowStart over, reset all:  "  << endl;
                maxTqmStep = 2 * simpleCC_QM_STEP;
                slowStartThreshold = 0;



            }

        }


        // slowStartStep = maximum possible step length
        EV_DEBUG << "slowStartStep :  " <<  maxTqmStep << endl;


        for (auto sr : records) {
                simpleCCResponseRecord * const r = dynamic_cast<simpleCCResponseRecord *>(sr);

                ASSERT(r);

                double  qm_mult_lower_cap = simpleCC_SLOW_START_THRESHOLD_MULT;
                if(r->getAvgQueueLength() > 0){
                    EV_DEBUG << "QueueLength greater 0 found = " << r->getAvgQueueLength() << endl;
                }
                double queueUtilization = r->getAvgQueueLength() / r->getQueueSize();
                EV_DEBUG << "queueUtilization = " << queueUtilization << endl;

                    // detect congestion and calculate reaction multiplier
                    if (queueUtilization > randomQueueThreshold && !oldCongestionBlocked){

                                slowStartThreshold = 0;

                                // save this value for the cubic function of the next congestion
                                lastCongestionQm = targetQM;
                                EV_DEBUG << "congestion happened by :  " <<  lastCongestionQm << endl;

                                qm_mult = 1 - std::min(simpleCC_HEAVY_CONGESTION_MULT * r->getAvgQueueLength() / r->getQueueSize(),  qm_mult_lower_cap);
                                EV_DEBUG << "heavy congestion qm mult :  " <<  qm_mult << endl;
                                // apply mupliplier
                                newQM = targetQM * qm_mult;

                                // set Qm to lower one
                                newTargetQM = std::min(newQM, newTargetQM);
                                EV_DEBUG << "heavy congestion newTargetQM found :  " <<  newTargetQM << endl;

                                lastCongestionMin = newTargetQM;
                                EV_DEBUG << "lastCongestionMin :  " <<  lastCongestionMin << endl;
                                // prevent congestion detection on next cycle and reset cycles since last congestion
                                congestionBlocked = true;
                                elapsedCycles = -1;


                            }
                            else{
                                // no congestion
                                if(newTargetQM >= targetQM &&  elapsedCycles >= 0 && queueUtilization <= randomQueueThreshold){
                                   EV_DEBUG << "last congestion qm :  " <<  lastCongestionQm << endl;
                                   congestionBlocked = false;

                                   EV_DEBUG << "time :  " <<  time << endl;

                                   // calculate new target qm with cubic function and apply caps
                                   newQM = simpleCC_c * pow(simpleCC_c * (time - k), 3.0) + lastCongestionQm;
                                   EV_DEBUG << "no congestion newQM :  " <<  newQM << endl;

                                   newQM = CLAMP(newQM, simpleCC_QM_MIN, simpleCC_QM_MAX);

                                   newQM = std::min(newQM, targetQM + maxTqmStep);
                                   EV_DEBUG << "no congestion, newQM :  " <<  newQM << endl;
                                   // apply slowstart caps
                                   if(targetQM < slowStartTH/2 && slowStartTH > 0){
                                       newQM = std::min(newQM, slowStartTH/2);
                                   }

                                   if(targetQM < slowStartTH && slowStartTH > 0){
                                       newQM = std::min(newQM, slowStartTH);
                                   }

                                   newTargetQM = newQM;
                                }

                            }

            }


        elapsedCycles++;
        EV_DEBUG << "lowest newTargetQM turn result " << newTargetQM << endl;
        newTargetQM = CLAMP(newTargetQM, simpleCC_QM_MIN, simpleCC_QM_MAX);

        handle->translator->setTargetQM(newTargetQM);
        EV_DEBUG << "Setting targetQM = " << newTargetQM << endl;

}



/*
 * Method which sets overhead values, updates the expected bit rate and schedules a pushTickerEvent.
 */
void simpleCCUDPTransport::simpleCCSetTranslator(SocketHandle_t * const handle, ICoCCTranslator* const translator) {
    auto oldPtr = handle->translator;

    handle->translator = translator;

    // set overhead values to allow for more precise calculations in translator
    translator->setPerPacketOverhead(lowerLayerOverhead);
    translator->setNetworkOverhead(metadataOverhead / collectionInterval.dbl());

    if (oldPtr == nullptr) {
        handle->pushPeriodStart = 0;
        handle->pushTicker = new cMessage("simpleCC push ticker event", simpleCC_PUSH_TICKER_MSG_KIND);
        handle->pushTicker->setContextPointer(handle);

        simpleCCSchedulePushTicker(handle);
    }
}
/*
 * Method, which initiates a Resonse using SerumSupport(Inet) by using a hopByHopHeader.
 */
void simpleCCUDPTransport::simpleCCReplyToMonitoringRequest(SocketHandle_t * const handle, NetworkOptions* &opts) {
    ASSERT(handle);

    if (handle->pendingRequest) {
        if (!opts) {
            opts = new NetworkOptions();
        }

        opts->setTrafficClass(TC_simpleCC);

        IPv6HopByHopOptionsHeader * hho = nullptr;

        const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);

        if (index >= 0) {
            hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(opts->getV6Header(index));
            ASSERT(hho);
        } else {
            hho = new IPv6HopByHopOptionsHeader();

            opts->addV6Header(hho);
        }

        SerumSupport::initiateResponse(handle->pendingRequest.get(), hho, DATASET_simpleCC_RESP);

        handle->pendingRequest.reset();
    }
}

void simpleCCUDPTransport::simpleCCSchedulePushTicker(SocketHandle_t * const handle) {
    // determine time of next push period
    handle->pushPeriodStart += collectionInterval;
    if (handle->pushPeriodStart + collectionInterval < simTime()) {
        // skipped push events due to inactive application, resync push interval
        handle->pushPeriodStart = static_cast<unsigned long>(simTime() / collectionInterval + 1) * collectionInterval;
        EV_DEBUG << "simpleCC push ticker resynchronized" << endl;
    }

    handle->forcedPushTime = handle->pushPeriodStart + (forcedPushSpread > SIMTIME_ZERO ? uniform(forcedPushDelay - forcedPushSpread, forcedPushDelay) : SIMTIME_ZERO);

    // reschedule push ticker
    cancelEvent(handle->pushTicker);
    scheduleAt(handle->forcedPushTime, handle->pushTicker);
}

bool simpleCCUDPTransport::simpleCCPushMetadata(SocketHandle_t * const handle, NetworkOptions* &opts) {
    ASSERT(handle);

    if (handle->translator && handle->pushPeriodStart <= simTime()) {
        const double qmActual = handle->translator->getActualQM();
        const double qmTarget = handle->translator->getTargetQM();

        if (pushSpread > SIMTIME_ZERO) {
            // randomize push time if it is likely that a later packet will follow within the push period
            const double maxRate = handle->translator->getMaxRate();
            const double actualRate = handle->translator->getRateForQM(qmActual, qmTarget);
            const double pMin = 1 - (actualRate / maxRate);
            const double pSend = (1 - pMin) / pushSpread.dbl() * (simTime() - handle->pushPeriodStart).dbl() + pMin;

            if (simTime() < handle->forcedPushTime && uniform(0, 1) > pSend) {
                return false;
            }
        }

        // add request record to query metadata
        simpleCCRequestRecord * const rr = new simpleCCRequestRecord();

        simpleCCSchedulePushTicker(handle);

        if (!opts) {
            opts = new NetworkOptions();
        }

        opts->setTrafficClass(TC_simpleCC);

        {
            IPv6DestinationOptionsHeader * doh = nullptr;

            const short index = opts->getV6HeaderIndex(IP_PROT_IPv6EXT_DEST);

            if (index >= 0) {
                auto doh = dynamic_cast<IPv6DestinationOptionsHeader *>(opts->getV6Header(index));
                ASSERT(doh);
            } else {
                doh = new IPv6DestinationOptionsHeader();

                opts->addV6Header(doh);
            }

            doh->getTlvOptions().add(rr);
        }

        EV_DEBUG << "simpleCC metadata request sent" << endl;

        return true;
    }

    return false;
}

void simpleCCUDPTransport::simpleCCHandlePushTicker(cMessage * const msg) {
    SocketHandle_t* const handle = (SocketHandle_t*) (msg->getContextPointer());

    ASSERT(handle);

    simpleCCHandlePushTicker(handle);
}
// potential edit in qm, need to reedit or skip
void simpleCCUDPTransport::simpleCCHandlePushTicker(SocketHandle_t * const handle) {
    //nl DG
    EV_INFO << "!!! Entered simpleCCHANDLEPUSHTICKER !!!" << endl;
    if (handle->connected && handle->inactivityCounter < simpleCC_INACTIVITY_THRESHOLD ) {
        inet::UDPSocket::SendOptions opts;

        handle->inactivityCounter++;

        // initiate push if not already done in period
        if (simpleCCPushMetadata(handle, opts.networkOptions)) {
            cPacket * const pkt = new simpleCCMetadataPushPkt("simpleCC metadata push packet", simpleCC_EMPTY_PUSH_MSG_KIND);

            handle->socket->sendTo(pkt, handle->remote, handle->port, &opts);
        }
    }

    // reschedule ticker timer one interval into future
    if (!handle->pushTicker->isScheduled()) {
        if (handle->inactivityCounter < simpleCC_INACTIVITY_THRESHOLD) {
            // required if connection is not established yet to continue probing
            simpleCCSchedulePushTicker(handle);
        } else {
            handle->pushPeriodStart = 0;

            EV_DEBUG << "simpleCC push ticker disabled due to inactivity" << endl;

            handle->translator->setTargetQM(0);

            EV_DEBUG << "Resetting target QM to 0" << endl;
        }
    }
}

simpleCCUDPTransport::SocketHandle_t * simpleCCUDPTransport::createSocket() {
    SocketHandle_t * const handle = new SocketHandle_t();

    handle->socket.reset(new UDPSocket());
    handle->socket->setOutputGate(udpOut);

    return handle;
}

void simpleCCUDPTransport::connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port) {
    ASSERT(handle);

    handle->socket->connect(endpAddr, port);

    handle->listening = false;
    handle->connected = false;
    handle->remote = endpAddr;
    handle->port = port;
}

void simpleCCUDPTransport::storeSocket(SocketHandle_t * const handle) {
    ASSERT(handle);

    // may fail for incoming connections tied to the listen socket
    // thats ok, we use the map in first place to retrieve the listen socket or
    // to check if a socket context has been established. thus no assert here
    connectionMap.insert(SocketMap_t::value_type(handle->socket->getSocketId(), handle));

    connectionVect.insert(connectionVect.end(), handle);
}

void simpleCCUDPTransport::initHandshake(SocketHandle_t * const handle, cMessage * const selfMsg) {
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
simpleCCUDPTransport::SocketHandle_t * simpleCCUDPTransport::getSocketById(const int id) {
    const auto it = connectionMap.find(id);

    if (it == connectionMap.end()) {
        EV_DEBUG << "getSocketById id not valid :  " << endl;
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
simpleCCUDPTransport::SocketHandle_t * simpleCCUDPTransport::getSocketByAddr(const L3Address& addr, const uint16_t port) {
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
