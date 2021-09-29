#include <string.h>
#include <algorithm>

#include "FCPConnection.h"
#include "FCP/contract/FCPCommand_m.h"
#include "FCP/serum/FCPSerumHeader_m.h"

#include "inet/networklayer/contract/IL3AddressType.h"
#include "inet/networklayer/contract/INetworkProtocolControlInfo.h"
#include "inet/networklayer/common/IPProtocolId_m.h"
#include "inet/common/INETUtils.h"
#include "inet/networklayer/contract/ipv4/IPv4ControlInfo.h"
#include "inet/networklayer/contract/ipv6/IPv6ControlInfo.h"
#include "inet/networklayer/serum/SerumSupport.h"

#define FCP_TICKER_MSG_KIND 1337
#define FCP_INACTIVITY_THRESHOLD 5

/**
 * Sends a new packet with the ack bit set.
 */
void FCPConnection::sendAck(FCPPacket* originalPacket, NetworkOptions* no){
    FCPPacket *packet = createFCPPacket("ACK");

    IPv6HopByHopOptionsHeader* hhoACK;

    //EV_DETAIL << "sendAck()" << endl;

    fillAckPacket(originalPacket, packet, no);

    if(no != nullptr){
        const short index = no->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);
        if(index >= 0){
            auto hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(no->getV6Header(index));

            if(SerumSupport::containsPush(hho, DATASET_FCP_PUSH)){
                const std::vector<SerumRecord *> records = SerumSupport::extractPush(hho, DATASET_FCP_PUSH);

                for (auto sr : records) {
                    FCPPushRecord * const r = dynamic_cast<FCPPushRecord *>(sr);

                    //EV_DETAIL << "Removed price: " << r->getPrice() << endl;

                    packet->setAckPrice(r->getPrice());

                    if (r->getCollectQM()) {
                        FCPResponseRecord* rr = createFCPResponseRecord();

                        hhoACK = createExtensionHeader(rr);

                        if (hho == nullptr) {
                            throw cRuntimeError("hho is nullptr");
                        }
                    } else {
                        hhoACK = nullptr;
                    }
                }
            } else {
                hhoACK = nullptr;
            }
        } else {
            hhoACK = nullptr;
        }
    } else {
        hhoACK = nullptr;
    }

    //EV_DETAIL << "Sending ACK packet with ack number: " << packet->getAckNo() << " and price: " << packet->getAckPrice() << "\n";
    sendToIp(packet, hhoACK);
}

/**
 * Sends a packet with the syn and ack bit set
 */
void FCPConnection::sendSynAck(FCPPacket* originalPacket){
    FCPPacket *packet = createFCPPacket("SYNACK");
    sequenceNo = rand() % 1000;
    fillAckPacket(originalPacket, packet, nullptr);
    packet->setSyn(true);
    //EV_DETAIL << "Sending SYN/ACK packet with ack number: " << packet->getAckNo() << " and ack price: " << packet->getAckPrice() << "\n";
    sendPacket(packet, nullptr);
}

/**
 * Returns true, if collectQM was set true in the FCPPushRecord and an FCPResponseRecord should be attached to the ACK
 */
void FCPConnection::fillAckPacket(FCPPacket* originalPacket, FCPPacket* ackPacket, NetworkOptions* no){
    ackPacket->setAck(true);
    ackPacket->setAckNo(originalPacket->getSequenceNo());
    ackPacket->setBalance(originalPacket->getBalance());
    ackPacket->setPreload(originalPacket->getPreload());
    ackPacket->setSize(originalPacket->getSize());

    //EV_DETAIL << "fillAckPacket: size: " << originalPacket->getSize() << endl;
}

/*
 * Sends a new packet with the fin bit set to the ip layer
 */
void FCPConnection::sendFin(){
    FCPPacket *packet = createFCPPacket("FIN");
    packet->setFin(true);
    sendToIp(packet, nullptr);
}

/*
 * Sends a new packet with the syn bit set to the ip layer
 */
void FCPConnection::sendSyn(){
    FCPPacket *packet = createFCPPacket("SYN");
    sequenceNo = rand() % 1000;
    packet->setSyn(true);
    //EV_DETAIL << "Sending SYN packet.\n";
    sendPacket(packet, nullptr);
}

void FCPConnection::sendEstabIndicationToApp(){
    cMessage *msg = new cMessage("FCP_I_ESTABLISHED");
    msg->setKind(FCP_I_ESTABLISHED);

    FCPConnectionInfo *indication = new FCPConnectionInfo();
    indication->setConnId(connId);
    indication->setLocalAddr(localAddr);
    indication->setRemoteAddr(remoteAddr);
    indication->setLocalPort(localPort);
    indication->setRemotePort(remotePort);

    msg->setControlInfo(indication);
    sendToApp(msg);
}


//Handle packets/commands from the application -----------------------------------

/*
 * Main function to handle all the commands coming from the application
 */
bool FCPConnection::processCommandFromApp(cMessage *msg){
    FCPCommand *fcpCommand = (FCPCommand *)(msg->removeControlInfo());
    FCPEventCode event = preanalyseAppCommandEvent(msg->getKind());

    switch (event) {
        case FCP_E_OPEN_ACTIVE:
            //Wants to open an active connection
            processOpenActive(event, fcpCommand, msg);
            break;

        case FCP_E_OPEN_PASSIVE:
            //Open a passive/listening connection
            processOpenPassive(event, fcpCommand, msg);
            break;

        case FCP_E_SEND:
            //Send data
            processSend(event, fcpCommand, msg);
            break;

        case FCP_E_CLOSE:
            //Close the connection
            processClose(event, fcpCommand, msg);
            break;

        case FCP_E_TRANSLATOR:
            processTranslator(event, fcpCommand, msg);
            break;

        case FCP_E_STREAMSTART:
            processTransportStreamStart(event, fcpCommand, msg);
            break;

        default:
            throw cRuntimeError(fcpMain, "wrong event code");
    }

    return performStateTransition(event);
}

/*
 * Changes the state of the connection based on the event that is occuring
 */
bool FCPConnection::performStateTransition(const FCPEventCode& event){
    if (event == FCP_E_IGNORE){
        return true;
    }

    switch(state.getState()){
    case FCP_S_INIT:
        switch(event){
        case FCP_E_OPEN_ACTIVE:
            FSM_Goto(state, FCP_S_SYN_SENT);
            break;
        case FCP_E_OPEN_PASSIVE:
            FSM_Goto(state, FCP_S_LISTEN);
            break;
        default:
            break;
        }
        break;

    case FCP_S_LISTEN:
        switch(event){
        case FCP_E_SEND:
        case FCP_E_OPEN_ACTIVE:
            FSM_Goto(state, FCP_S_SYN_SENT);
            break;
        case FCP_E_CLOSE:
            FSM_Goto(state, FCP_S_CLOSED);
            break;
        case FCP_E_RCV_SYN:
            FSM_Goto(state, FCP_S_SYN_ACK_SENT);
            break;
        default:
            break;
        }
        break;

    case FCP_S_SYN_SENT:
        switch (event) {
            case FCP_E_CLOSE:
                FSM_Goto(state, FCP_S_CLOSED);
                break;

            case FCP_E_RCV_SYN_ACK:
                FSM_Goto(state, FCP_S_ESTABLISHED);
                break;

            default:
                break;
        }
        break;

    case FCP_S_SYN_ACK_SENT:
        switch (event) {
            case FCP_E_RCV_ACK:
                FSM_Goto(state, FCP_S_ESTABLISHED);
                break;

            default:
                break;
        }
        break;

    case FCP_S_ESTABLISHED:
        switch (event) {
            case FCP_E_CLOSE:
                FSM_Goto(state, FCP_S_CLOSED);
                break;
            case FCP_E_RCV_FIN:
                FSM_Goto(state, FCP_S_CLOSED);
                break;

            default:
                break;
        }
        break;
    }
    return state.getState() != FCP_S_CLOSED;
}

/*
 * Transform the application commands into event commands
 */
FCPEventCode FCPConnection::preanalyseAppCommandEvent(int commandCode){
    switch (commandCode) {
        case FCP_C_OPEN_ACTIVE:
            return FCP_E_OPEN_ACTIVE;

        case FCP_C_OPEN_PASSIVE:
            return FCP_E_OPEN_PASSIVE;

        case FCP_C_SEND:
            return FCP_E_SEND;

        case FCP_C_CLOSE:
            return FCP_E_CLOSE;

        case FCP_C_TRANSLATOR:
            return FCP_E_TRANSLATOR;

        case FCP_C_STREAMSTART:
            return FCP_E_STREAMSTART;

        default:
            throw cRuntimeError(fcpMain, "Unknown message kind in app command");
    }
}

/*
 * Opens a new active connection
 */
void FCPConnection::processOpenActive(FCPEventCode& event, FCPCommand *command, cMessage *msg){
    FCPOpenCommand *openCmd = check_and_cast<FCPOpenCommand *>(command);
    L3Address localAddr, remoteAddr;
    int localPort, remotePort;

    if(state.getState() == FCP_S_INIT){
        localAddr = openCmd->getLocalAddr();
        remoteAddr = openCmd->getRemoteAddr();
        localPort = openCmd->getLocalPort();
        remotePort = openCmd->getRemotePort();
        function = openCmd->getFunction();
        percentage = openCmd->getPercentage();
        usePreload = openCmd->getUsePreload();
        collectionInterval = openCmd->getCollectionInterval();
        forcedPushDelay = openCmd->getForcedPushDelay();
        fcpEpsilon = openCmd->getFcpEpsilon();
        fcpQ = openCmd->getFcpQ();

        metadataOverhead = openCmd->getMetadataOverhead();
        lowerLayerOverhead = openCmd->getLowerLayerOverhead();
        fcpOverhead = openCmd->getFcpOverhead();

        enableQMSmoothing = openCmd->getEnableQMSmoothing();
        qmHistoryLength = openCmd->getQmHistoryLength();

        //EV_DETAIL << "MOH: " << metadataOverhead << ", LLOH: " << lowerLayerOverhead << ", FOH: " << fcpOverhead << endl;

        if(remoteAddr.isUnspecified() || remotePort == -1){
            throw cRuntimeError(fcpMain, "Error: Please assign a remote address and port.");
        }

        if(localPort == -1){
            localPort = fcpMain->getEphemeralPort();
            //EV_DETAIL << "Assigned ephemeral port " << localPort << "\n";
        }

        fcpMain->addSocketPair(this, localAddr, remoteAddr, localPort, remotePort);
        sendQueue = new FCPSendQueue();
        sendQueue->setConnection(this);

        //EV_DETAIL << "OPEN: " << localAddr << ":" << localPort << " --> " << remoteAddr << ":" << remotePort << "\n";

        sendSyn();
    }
    else{
        throw cRuntimeError(fcpMain, "Error: Connection already exists. Can't open another active connection.");
    }
    delete openCmd;
    delete msg;
}

/*
 * Opens a new passive/listening connection
 */
void FCPConnection::processOpenPassive(FCPEventCode& event, FCPCommand *command, cMessage *msg){
    FCPOpenCommand *openCmd = check_and_cast<FCPOpenCommand *>(command);
    L3Address localAddr;
    int localPort;

    if(state.getState() == FCP_S_INIT){
        localAddr = openCmd->getLocalAddr();
        localPort = openCmd->getLocalPort();
        function = openCmd->getFunction();
        percentage = openCmd->getPercentage();
        usePreload = openCmd->getUsePreload();
        collectionInterval = openCmd->getCollectionInterval();
        forcedPushDelay = openCmd->getForcedPushDelay();
        fcpEpsilon = openCmd->getFcpEpsilon();
        fcpQ = openCmd->getFcpQ();

        metadataOverhead = openCmd->getMetadataOverhead();
        lowerLayerOverhead = openCmd->getLowerLayerOverhead();
        fcpOverhead = openCmd->getFcpOverhead();

        enableQMSmoothing = openCmd->getEnableQMSmoothing();
        qmHistoryLength = openCmd->getQmHistoryLength();

        //EV_DETAIL << "MOH: " << metadataOverhead << ", LLOH: " << lowerLayerOverhead << ", FOH: " << fcpOverhead << endl;

        if(localPort == -1){
            throw cRuntimeError(fcpMain, "Error: To open a passive connection please enter a port to listen on.");
        }

        fcpMain->addSocketPair(this, localAddr, L3Address(), localPort, -1);
        sendQueue = new FCPSendQueue();
        sendQueue->setConnection(this);

        //EV_DETAIL << "Listen on: " << localAddr << ":" << localPort << "\n";
    }
    else{
        throw cRuntimeError(fcpMain, "Error: Connection already exists. Can't open another passive connection.");
    }
    delete openCmd;
    delete msg;
}

/*
 * Sends data over a connection
 */
void FCPConnection::processSend(FCPEventCode& event, FCPCommand *command, cMessage *msg){
    FCPSendCommand *sendCmd = check_and_cast<FCPSendCommand *>(command);

    switch(state.getState()){
    case FCP_S_INIT:
        throw cRuntimeError(fcpMain, "Error: Can't send data. Connection not open.");

    case FCP_S_LISTEN:
        //EV_DETAIL << "Send command turns the connection from a passive to an active one.\n";
        sendSyn();
        sendQueue->enqueueData(PK(msg));
        break;

    case FCP_S_SYN_ACK_SENT:
    case FCP_S_SYN_SENT:
        //EV_DETAIL << "Adding the message to the send queue.\n";
        sendQueue->enqueueData(PK(msg));
        break;
    case FCP_S_ESTABLISHED:
        //EV_DETAIL << "Sending data.\n";
        inactivityCounter = 0;
        sendQueue->enqueueData(PK(msg));
        sendData();
        break;
    default:
        throw cRuntimeError(fcpMain, "Error: Connection is in the wrong state to send data.");
    }

    delete sendCmd;
}

/*
 * Closes the connection
 */
void FCPConnection::processClose(FCPEventCode& event, FCPCommand *command, cMessage *msg){
    delete command;
    delete msg;
    if(!sendQueue->isEmpty()){
        sendData();
    }
    //EV_DETAIL << "Closing connection.\n";
    sendFin();
}

void FCPConnection::processTranslator(FCPEventCode& event, FCPCommand* command, cMessage* msg){
    FCPTranslatorCommand *cmd = check_and_cast<FCPTranslatorCommand *>(command);
    translator = cmd->getTranslator();

    pushPeriodStart = static_cast<unsigned long>(simTime() / collectionInterval) * collectionInterval;
    pushTicker = new cMessage("FCP push ticker event", FCP_TICKER_MSG_KIND);
    pushTicker->setContextPointer(this);
    //fcpMain->scheduleAt(pushPeriodStart + forcedPushDelay, pushTicker);

    translator->setTargetQM(0);

    translator->setNetworkOverhead(metadataOverhead);
    translator->setPerPacketOverhead(lowerLayerOverhead);

    delete command;
    delete msg;
}

void FCPConnection::processTransportStreamStart(FCPEventCode& event, FCPCommand* command, cMessage* msg){
    FCPTransportStreamStartCommand *cmd = check_and_cast<FCPTransportStreamStartCommand *>(command);
    startTime = cmd->getStart();

    EV_INFO << "Start time is: " << startTime << endl;

    // do nothing currently

    delete command;
    delete msg;
}

//Handle packets from IP ------------------------------------------


/*
 * Main function which handles incoming packets from the ip layer
 */
bool FCPConnection::processFCPPacket(FCPPacket *packet, L3Address srcAddr, L3Address destAddr, NetworkOptions* no){
    FCPEventCode event;

    switch(state.getState()){
    case FCP_S_LISTEN:
        event = processPacketListen(packet, srcAddr, destAddr);
        break;
    case FCP_S_SYN_SENT:
        event = processPacketSynSent(packet, srcAddr, destAddr);
        break;
    case FCP_S_SYN_ACK_SENT:
        event = processPacketSynAckSent(packet, srcAddr, destAddr);
        break;
    default:
        event = processPacket(packet, no);
    }
    bool close = performStateTransition(event);
    if(state.getState() == FCP_S_ESTABLISHED){
        sendData();
    }
    delete packet;
    return close;
}

/*
 * What happens with a packet when the connection is set to listen
 */
FCPEventCode FCPConnection::processPacketListen(FCPPacket *packet, L3Address srcAddr, L3Address destAddr){
    if(packet->getSyn()){
        //EV_DETAIL << "Syn received while listening. Sending SynAck. Updating socket.\n";
        fcpMain->updateSocketPair(this, destAddr, srcAddr, packet->getDestPort(), packet->getSrcPort());
        sendSynAck(packet);
        return FCP_E_RCV_SYN;
    }
    return FCP_E_IGNORE;
}

/*
 * What happens when a syn has been sent and a packet arrives
 */
FCPEventCode FCPConnection::processPacketSynSent(FCPPacket *packet, L3Address srcAddr, L3Address destAddr){
    if(packet->getAck() && packet->getSyn()){
        //EV_DETAIL << "SynAck received after syn was sent. Connection established and sending Ack.\n";
        receivedAck(packet, nullptr);
        fcpMain->updateSocketPair(this, destAddr, srcAddr, packet->getDestPort(), packet->getSrcPort());
        sendAck(packet, nullptr);
        sendEstabIndicationToApp();
        return FCP_E_RCV_SYN_ACK;
    }
    return FCP_E_IGNORE;
}

FCPEventCode FCPConnection::processPacketSynAckSent(FCPPacket *packet, L3Address srcAddr, L3Address destAddr){
    if(packet->getAck()){
        //EV_DETAIL << "Ack received after SynAck was sent. Connection established.\n";
        receivedAck(packet, nullptr);
        sendEstabIndicationToApp();
        return FCP_E_RCV_ACK;
    }
    return processData(packet, nullptr);
}

/**
 * Default case, when a packet is received and none of the above functions gets called
 */
FCPEventCode FCPConnection::processPacket(FCPPacket *packet, NetworkOptions* no){

    //EV_DETAIL << "processPacket" << endl;

    if(state.getState() == FCP_S_ESTABLISHED){
        if(packet->getFin()){
            fcpMain->close(this);
            //EV_DETAIL << "Received fin and the send queue is empty. Closing connection.";
            return FCP_E_RCV_FIN;
        } else if(packet->getAck()){
            //EV_DETAIL << "Received Ack.";
            receivedAck(packet, no);
            return FCP_E_RCV_ACK;
        } else if (packet->hasEncapsulatedPacket()) {
            return processData(packet, no);
        } else {
            return processMetadataPacket(packet, no);
        }
    }
    return FCP_E_IGNORE;
}

FCPEventCode FCPConnection::processData(FCPPacket *packet, NetworkOptions* no){
    //EV_DETAIL << "Received data. Sending it to the application. And sending Ack.\n";
    cPacket * msg = packet->decapsulate();
    FCPCommand *cmd = new FCPCommand();
    cmd->setConnId(connId);
    msg->setControlInfo(cmd);
    msg->setKind(FCP_I_DATA);
    sendAck(packet, no);
    sendToApp(msg);
    return FCP_E_RCV_DATA;
}

FCPEventCode FCPConnection::processMetadataPacket(FCPPacket *packet, NetworkOptions* no) {
    //EV_DETAIL << "Received empty metadata packet. Sending Ack.\n";
    sendAck(packet, no);
    return FCP_E_IGNORE;
}


//Utility ---------------------------------------------------

void FCPConnection::handlePushTickerEvent() {

    //EV_INFO << "pushTicker fired" << endl;

    if (inactivityCounter < FCP_INACTIVITY_THRESHOLD ) {

        inactivityCounter++;

        IPv6HopByHopOptionsHeader* hho;


        // initiate push if not already done in period
        if (pushMetadata(hho, fcpOverhead * 8) == true) {
            FCPPacket* packet = createFCPPacket("EMPTYPUSH");

            sendPacket(packet, hho);
            //EV_DETAIL << "Pushing empty metadata packet" << endl;
        }
    }

    // reschedule ticker timer one interval into future
    if (!pushTicker->isScheduled()) {
        if (inactivityCounter < FCP_INACTIVITY_THRESHOLD) {
            fcpMain->scheduleAt(simTime() + collectionInterval, pushTicker);
        } else {
            //EV_INFO << "FCP push ticker disabled due to inactivity" << endl;
            translator->setTargetQM(0);
        }
    }
}

bool FCPConnection::pushMetadata(IPv6HopByHopOptionsHeader* &hho, int size) {

    FCPPushRecord* pr;

    // compute current metadata and append them
    if (translator != nullptr && pushPeriodStart <= simTime()) {

        targetQM = translator->getTargetQM();

        pr = createFCPPushRecord(size, true);

        pushedQMValues.push_front(translator->getTargetQM());
        pushedQMValues.pop_back();

        pushedBNQMValues.push_front(avgBNQMToAnnounce);
        pushedBNQMValues.pop_back();

        EV_DETAIL << "pushedQMValues front: " << pushedQMValues.front() << ", back: " << pushedQMValues.back() << endl;
        EV_DETAIL << "pushMetadata targetQM: " << targetQM << ", BN QM: " << avgBNQMToAnnounce << ", flows: " << bottleneckFlows << endl;



        hho = createExtensionHeader(pr);

        // determine time of next push period
        pushPeriodStart += collectionInterval;
        if (pushPeriodStart + collectionInterval < simTime()) {
            // skipped push events due to inactive application, resync push interval
            pushPeriodStart = static_cast<unsigned long>(simTime() / collectionInterval + 1) * collectionInterval;
            //EV_INFO << "FCP push ticker resynchronized" << endl;
        }
        // reschedule push ticker
        fcpMain->cancelEvent(pushTicker);
        fcpMain->scheduleAt(pushPeriodStart + forcedPushDelay, pushTicker);

        return true;

    } else {

        pr = createFCPPushRecord(size, false);
        hho = createExtensionHeader(pr);

        return false;

    }

}


IPv6HopByHopOptionsHeader* FCPConnection::createExtensionHeader(SerumRecord* sr) {

    IPv6HopByHopOptionsHeader* head = new IPv6HopByHopOptionsHeader();
    head->getTlvOptions().add(sr);
    return head;
}

FCPPushRecord* FCPConnection::createFCPPushRecord(int size, bool collectQM) {
    FCPPushRecord* pr = new FCPPushRecord();
    pr->setSize(size);
    pr->setPreload(preload);
    pr->setPrice(0);
    pr->setRtt(rtt);

    pr->setCollectQM(collectQM);
    pr->setBottleneckQM(avgBNQMToAnnounce);
    pr->setBudget(budget);
    pr->setTargetQM(targetQM);
    pr->setFlows(bottleneckFlows);

    return pr;
}

FCPResponseRecord* FCPConnection::createFCPResponseRecord() {
    FCPResponseRecord* rr = new FCPResponseRecord();
    rr->setAverageBottleneckBudget(-1);
    rr->setAverageBottleneckQM(-1);
    rr->setFlows(-1);
    rr->setDataDesc(-DATASET_FCP_RESP);
    rr->setType(MONITORING_HH_APPEND);

    return rr;
}

/**
 * Starts to send the data in the sending queue. Once invoked, this method will sent packets until the send queue is empty.
 */
void FCPConnection::sendData(){
    while(!sendQueue->isEmpty()){
        FCPPacket* packet = sendQueue->createPacket();
        int size = (packet->getByteLength() + fcpOverhead)*8; //add some constant
        packet->setSize(size);

        //EV_DETAIL << "Packet bytes: " << packet->getByteLength() << ", size: " << size << endl;

        IPv6HopByHopOptionsHeader* hho;

        pushMetadata(hho, size);

        if (hho == nullptr) {
            throw cRuntimeError("sendData() hho == nullptr");
        }

        sendPacket(packet, hho);
    }
}

void FCPConnection::sendPacket(FCPPacket *packet, IPv6HopByHopOptionsHeader* hho){
    packet->setSequenceNo(sequenceNo);
    packet->setRtt(rtt);
    packet->setPrice(0);
    packet->setBalance(price);
    timePacketSent[sequenceNo] = simTime().dbl();
    packet->setPreload(preload);
    preload = 0;

    //EV_DETAIL << "Sending packet with sequence number: " << sequenceNo << " and preload: " << preload << ".\n";
    sequenceNo += 1;
    sendToIp(packet, hho);
}

void FCPConnection::receivedAck(FCPPacket* packet, NetworkOptions* no) {
    int ackNumber = packet->getAckNo();
    price = packet->getAckPrice();

    if(no != nullptr){
        const short index = no->getV6HeaderIndex(IP_PROT_IPv6EXT_HOP);
        if(index >= 0){
            auto hho = dynamic_cast<IPv6HopByHopOptionsHeader *>(no->getV6Header(index));
            if(SerumSupport::containsResponse(hho, DATASET_FCP_RESP)){
                const std::vector<SerumRecord *> records = SerumSupport::extractResponse(hho, DATASET_FCP_RESP);

                avgBNQMToAnnounce = -1;
                avgBNQMToCalculate = -1;
                avgBNBudgetToCalculate = -1;

                for (auto sr : records) {
                    FCPResponseRecord * const r = dynamic_cast<FCPResponseRecord *>(sr);

                    double pushedBNQM = pushedBNQMValues.back();
                    double avgBNQM = r->getAverageBottleneckQM();
                    double prevAvgBNQM = r->getPrevAvgBottleneckQM();
                    double avgBNBudget = r->getAverageBottleneckBudget();
                    double flowsBN = r->getFlows();

                    EV_DETAIL << "Received ACK containing a FCPResponseRecord" << endl;
                    EV_DETAIL << "avgBNQM: " << avgBNQM << ", prevAvgBNQM: " << prevAvgBNQM << ", pushedBNQM: " << pushedBNQMValues.back() << endl;
                    EV_DETAIL << "avgBNBudget: " << avgBNBudget << ", flows: " << flowsBN << endl;

                    if (avgBNQM != -1 && avgBNBudget != -1) {

                        // avgBNQMToAnnounce is minimum of all avgBNQMs
                        if (avgBNQM <= avgBNQMToAnnounce || avgBNQMToAnnounce == -1) {

                            avgBNQMToAnnounce = avgBNQM;
                        }

                        // check if previous push was used for calculating avgBNQM
                        if (pushedBNQM >= prevAvgBNQM && (avgBNQM <= avgBNQMToCalculate || avgBNQMToCalculate == -1)) {

                            avgBNQMToCalculate = avgBNQM;
                            avgBNBudgetToCalculate = avgBNBudget;
                            bottleneckFlows = flowsBN;
                        }

                        EV_DETAIL << "Updated avgBNQMToAnnounce: " << avgBNQMToAnnounce << ", avgBNQMToCalculate" << avgBNQMToCalculate << ", avgBNBudgetToCalculate: " << avgBNBudgetToCalculate << endl;

                    }
                }

                if (avgBNQMToAnnounce != -1 && avgBNQMToCalculate != -1) {
                    fcpMain->emit(fcpMain->avgBNQMToCalcSignal, avgBNQMToCalculate);
                    fcpMain->emit(fcpMain->avgBNQMToAnnounceSignal, avgBNQMToAnnounce);
                    fcpMain->emit(fcpMain->avgBNBudgetSignal, avgBNBudgetToCalculate);
                    fcpMain->emit(fcpMain->numBNFlowsSignal, bottleneckFlows);
                }
                // only update budget if ACK contained response
                updateBudget();
            }
        }
    }


    //EV_DETAIL << "Received ACK with number: " << ackNumber << ". The new price is: " << price << ".\n";
    if(timePacketSent.find(ackNumber) != timePacketSent.end()){
        double t = simTime().dbl();
        rtt = (t - timePacketSent[ackNumber]);
        for(int i = lastAckedPacket+1; i <= ackNumber; i++){
            timePacketSent.erase(i);
        }
        //EV_DETAIL << "The new RTT is: " << rtt << ".\n";
        lastAckedPacket = ackNumber;

        /**
         * With the start of a connection the price isn't known, so we start with a base
         * sending rate and when the first price is received we calculate the budget.
         */
        //if(budget == 0 && price > 0){
        //    budget = price*baseSendingRate;
        //}

        if(packet->getPreload() != 0){

            double budgetChange = packet->getBalance() * packet->getSize() * (packet->getPreload()/rtt);

            budget += budgetChange;
            //EV_DETAIL << "Preload Updated budget to: " << budget << ", amount: " << budgetChange << ", preload is: " << packet->getPreload() << endl;
            //EV_DETAIL << "Balance: " << packet->getBalance() << ", size: " << packet->getSize() << ", preload: " << packet->getPreload() << ", rtt: " << rtt << endl;
        }
        updateSendingRate();
    }
}

void FCPConnection::updateBudget(){
    if(translator != nullptr){
        double newBudget = budget;



        switch(function){
        case 0:
            //EV_DETAIL << "Using constant budget" << endl;
            break;
        case 1:

            if (avgBNQMToCalculate != -1 && avgBNBudgetToCalculate != -1) {

                if (avgBNQMToCalculate != 0 && pushedQMValues.back() != avgBNQMToCalculate) {

                    double diff = pushedQMValues.back() - avgBNQMToCalculate;
                    double unf = diff / avgBNQMToCalculate;

                    double budgetChange = avgBNBudgetToCalculate * unf * fcpQ;
                    newBudget = budget - budgetChange;

                    if (newBudget < 1) {
                        newBudget = 1;
                    }

                    fcpMain->emit(fcpMain->changedBudgetSignal, -budgetChange);
                    fcpMain->emit(fcpMain->qmDiffSignal, diff);


                    EV_DETAIL << "pushedQM old: " << pushedQMValues.back() << ", pushedQM new: " << pushedQMValues.front() << ", avgBNQM: " << avgBNQMToCalculate << ", pushed avgBNQM: " << pushedBNQMValues.back() << endl;
                    EV_DETAIL << "budgetChange: " << -budgetChange << ", old budget: " << budget << ", new budget: " << newBudget << endl;

                } else {
                    EV_DETAIL << "Can't update budget, avgBottleneckQM is: " << avgBNQMToCalculate << " and pushedTargetQocOld is: " << pushedQMValues.back() << " and pushedTargetQocNew is: " << pushedQMValues.front() << endl;
                }

            } else {
                //EV_DETAIL << "Can't update budget, avgBottleneckQM is: " << avgBottleneckQM << " and avgBottleneckBudget is: " << avgBNBudget << endl;
            }

            break;
        default:
            break;
        }

        if(newBudget < budget || !usePreload){
        //if(!usePreload){
            budget = newBudget;
            //EV_DETAIL << "Updated budget to: " << newBudget << endl;
        } else{
            preload = (newBudget-budget)/budget;
            //EV_DETAIL << "Updated budget to: " << newBudget << ", preload is: " << preload << " , old budget is: " << budget << endl;
        }
    } else{
        //EV_DETAIL << "Translator not set. Can't update budget." << endl;
    }
}

void FCPConnection::updateSendingRate() {

    if(translator != nullptr) {

        if (price == 0) {

            fcpMain->emit(fcpMain->priceEndpSignal, price);
            fcpMain->emit(fcpMain->budgetSignal, budget);

            const double actualTargetQM = translator->getTargetQM();

            //EV_DETAIL << "Price is 0, actualTargetQM: " << actualTargetQM << ", new targetQM: " << 1 << endl;

            translator->setTargetQM(1);

        } else {

            double rate = budget/price;

            fcpMain->emit(fcpMain->rateSignal, rate);
            fcpMain->emit(fcpMain->priceEndpSignal, price);
            fcpMain->emit(fcpMain->budgetSignal, budget);

            if(rate>0) {

                const double actualQM = translator->getActualQM();

                double targetQMForRate = translator->getQMForRate(actualQM, rate);

                double newTargetQM;

                if (!enableQMSmoothing) {
                    newTargetQM = targetQMForRate;

                } else {
                    addQMValue(targetQMForRate);
                    const double avgTargetQM = getAverageQM();
                    fcpMain->emit(fcpMain->avgTargetQMSignal, avgTargetQM);
                    newTargetQM = avgTargetQM;
                }

                if (newTargetQM < 0) {
                    newTargetQM = 0;
                }

                translator->setTargetQM(newTargetQM);

                targetQM = translator->getTargetQM();

                //EV_DETAIL << "budget: " << budget << ", price: " << price << ", rate: " << rate << endl;
                //EV_DETAIL << "targetQMForRate: " << targetQMForRate << ", newTargetQM: " << newTargetQM << endl;

                //EV_DETAIL << "Updated the targetQM to: " << targetQM << " with rate: " << rate << endl;
            }
        }

    } else {
        //EV_DETAIL << "Translator not set. Can't update targetQM." << endl;
    }
}

/*
 * Adds ports and addresses to the packet and sends it packet down to the ip layer
 */
void FCPConnection::sendToIp(FCPPacket *packet, IPv6HopByHopOptionsHeader* hho){

    packet->setSourcePort(localPort);
    packet->setDestinationPort(remotePort);


    //EV_DETAIL << "Sending the packet from port: " << localPort << " to port: " << remotePort << ".\n";

    if (remoteAddr.getType() == L3Address::IPv4) {
        // send to IPv4
        IPv4ControlInfo *ipControlInfo = new IPv4ControlInfo();
        ipControlInfo->setProtocol(IP_PROT_FCP);
        ipControlInfo->setSrcAddr(localAddr.toIPv4());
        ipControlInfo->setDestAddr(remoteAddr.toIPv4());
        packet->setControlInfo(ipControlInfo);

    }
    else if (remoteAddr.getType() == L3Address::IPv6) {
        // send to IPv6
        IPv6ControlInfo *ipControlInfo = new IPv6ControlInfo();
        ipControlInfo->setProtocol(IP_PROT_FCP);
        ipControlInfo->setSrcAddr(localAddr.toIPv6());
        ipControlInfo->setDestAddr(remoteAddr.toIPv6());
        if(hho != nullptr){
            ipControlInfo->addExtensionHeader(hho);
        }
        packet->setControlInfo(ipControlInfo);
    }

    //EV_DETAIL << "Packet bytes sendToIp: " << packet->getByteLength() << endl;

    fcpMain->send(packet, "ipOut");
}

/*
 * Creates a new fcp packet
 */
FCPPacket *FCPConnection::createFCPPacket(const char *name){
    return new FCPPacket(name);
}

/*
 * Sends a message to the application
 */
void FCPConnection::sendToApp(cMessage *msg){
    fcpMain->send(msg, "appOut", appGateIndex);
}

void FCPConnection::addQMValue(double qm) {
    targetQMHistory.push_back(qm);

    while (targetQMHistory.size() > qmHistoryLength) {
        targetQMHistory.pop_front();
    }
}

double FCPConnection::getAverageQM() {
    double sum = 0;

    for (std::list<double>::iterator it = targetQMHistory.begin(); it != targetQMHistory.end(); it++) {
        sum += *it;
    }

    return sum / targetQMHistory.size();
}

FCPConnection::FCPConnection(FCP *_main, int _appGateIndex, int _connId){
    fcpMain = _main;
    appGateIndex = _appGateIndex;
    connId = _connId;
    budget = 1000;
    rtt = 0;
    price = 0;
    preload = 0;
    lastAckedPacket = -1;
    sequenceNo = 0;
    function = 0;
    percentage = 0.1;

    avgBNQMToAnnounce = -1;
    avgBNQMToCalculate = -1;
    avgBNBudgetToCalculate = -1;
    targetQM = 0;

    pushedQMValues.push_front(-1);
    pushedQMValues.push_front(-1);

    pushedBNQMValues.push_front(-1);
    pushedBNQMValues.push_front(-1);
}

FCPConnection::~FCPConnection(){
    if (pushTicker && !pushTicker->isScheduled()) {
        delete pushTicker;
        pushTicker = nullptr;
    }

    if (sendQueue) {
        delete sendQueue;
    }

}
