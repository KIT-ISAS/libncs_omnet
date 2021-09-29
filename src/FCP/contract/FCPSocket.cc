#include "FCPSocket.h"

void FCPSocket::sendToFCP(cMessage *msg){
    if (!gateToFcp)
        throw cRuntimeError("FCPSocket: setOutputGate() must be invoked before socket can be used");

    check_and_cast<cSimpleModule *>(gateToFcp->getOwnerModule())->send(msg, gateToFcp);
}

FCPSocket::FCPSocket(int _function, double _percentage, bool _usePreload, simtime_t _collectionInterval, simtime_t _forcedPushDelay, double _fcpEpsilon, double _fcpQ,
        int _metadataOverhead, int _lowerLayerOverhead, int _fcpOverhead, bool _enableQMSmoothing, int _qmHistoryLength) {
    connId = getEnvir()->getUniqueNumber();
    gateToFcp = nullptr;
    localPort = remotePort = -1;
    function = _function;
    percentage = _percentage;
    usePreload = _usePreload;
    collectionInterval = _collectionInterval;
    forcedPushDelay = _forcedPushDelay;
    fcpEpsilon = _fcpEpsilon;
    fcpQ = _fcpQ;

    metadataOverhead = _metadataOverhead;
    lowerLayerOverhead = _lowerLayerOverhead;
    fcpOverhead = _fcpOverhead;

    enableQMSmoothing = _enableQMSmoothing;
    qmHistoryLength = _qmHistoryLength;
}

FCPSocket::~FCPSocket(){

}

void FCPSocket::bind(int port){
    localPort = port;
}

void FCPSocket::listen(){
    cMessage *msg = new cMessage("PassiveOPEN", FCP_C_OPEN_PASSIVE);

    FCPOpenCommand *openCmd = new FCPOpenCommand();
    openCmd->setLocalAddr(localAddr);
    openCmd->setLocalPort(localPort);
    openCmd->setConnId(connId);
    openCmd->setFunction(function);
    openCmd->setPercentage(percentage);
    openCmd->setUsePreload(usePreload);

    openCmd->setCollectionInterval(collectionInterval);
    openCmd->setForcedPushDelay(forcedPushDelay);
    openCmd->setFcpEpsilon(fcpEpsilon);
    openCmd->setFcpQ(fcpQ);

    openCmd->setMetadataOverhead(metadataOverhead);
    openCmd->setLowerLayerOverhead(lowerLayerOverhead);
    openCmd->setFcpOverhead(fcpOverhead);

    openCmd->setEnableQMSmoothing(enableQMSmoothing);
    openCmd->setQmHistoryLength(qmHistoryLength);

    msg->setControlInfo(openCmd);
    sendToFCP(msg);
}

void FCPSocket::connect(L3Address rAddr, int rPort){
    cMessage *msg = new cMessage("ActiveOPEN", FCP_C_OPEN_ACTIVE);

    remoteAddr = rAddr;
    remotePort = rPort;

    FCPOpenCommand *openCmd = new FCPOpenCommand();
    openCmd->setConnId(connId);
    openCmd->setLocalAddr(localAddr);
    openCmd->setLocalPort(localPort);
    openCmd->setRemoteAddr(remoteAddr);
    openCmd->setRemotePort(remotePort);
    openCmd->setFunction(function);
    openCmd->setPercentage(percentage);
    openCmd->setUsePreload(usePreload);
    openCmd->setCollectionInterval(collectionInterval);
    openCmd->setForcedPushDelay(forcedPushDelay);
    openCmd->setFcpEpsilon(fcpEpsilon);
    openCmd->setFcpQ(fcpQ);

    openCmd->setMetadataOverhead(metadataOverhead);
    openCmd->setLowerLayerOverhead(lowerLayerOverhead);
    openCmd->setFcpOverhead(fcpOverhead);

    openCmd->setEnableQMSmoothing(enableQMSmoothing);
    openCmd->setQmHistoryLength(qmHistoryLength);

    msg->setControlInfo(openCmd);
    sendToFCP(msg);
}

void FCPSocket::send(cMessage *msg){
    msg->setKind(FCP_C_SEND);
    FCPSendCommand *cmd = new FCPSendCommand();
    cmd->setConnId(connId);
    msg->setControlInfo(cmd);
    sendToFCP(msg);
}

void FCPSocket::sendCommand(cMessage *msg){
    sendToFCP(msg);
}

void FCPSocket::close(){
    cMessage *msg = new cMessage("CLOSE", FCP_C_CLOSE);
    FCPCommand *cmd = new FCPCommand();
    cmd->setConnId(connId);
    msg->setControlInfo(cmd);
    sendToFCP(msg);
}

void FCPSocket::processMessage(cMessage* msg){
    FCPConnectionInfo* connectionInfo;
    switch(msg->getKind()){
        case FCP_I_ESTABLISHED:
            connectionInfo = check_and_cast<FCPConnectionInfo *>(msg->getControlInfo());
            localAddr = connectionInfo->getLocalAddr();
            remoteAddr = connectionInfo->getRemoteAddr();
            localPort = connectionInfo->getLocalPort();
            remotePort = connectionInfo->getRemotePort();
            break;
        default:
            break;
    }
}

void FCPSocket::setTranslator(ICoCCTranslator* trans){
    translator = trans;
    sendTranslatorCommand();
}

void FCPSocket::sendTranslatorCommand(){
    cMessage *msg = new cMessage("TranslatorSET", FCP_C_TRANSLATOR);

    FCPTranslatorCommand *cmd = new FCPTranslatorCommand();
    cmd->setTranslator(translator);
    cmd->setConnId(connId);

    msg->setControlInfo(cmd);
    sendToFCP(msg);
}

void FCPSocket::setTransportStreamStart(simtime_t start) {
    startTime = start;
    sendTransportStreamStartCommand();
}

void FCPSocket::sendTransportStreamStartCommand(){
    cMessage *msg = new cMessage("StreamstartSET", FCP_C_STREAMSTART);

    FCPTransportStreamStartCommand *cmd = new FCPTransportStreamStartCommand();
    cmd->setStart(startTime);
    cmd->setConnId(connId);

    msg->setControlInfo(cmd);
    sendToFCP(msg);
}
