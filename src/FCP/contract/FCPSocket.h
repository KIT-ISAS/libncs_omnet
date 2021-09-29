#ifndef __LIBNCS_OMNET_FCPSOCKET_H
#define __LIBNCS_OMNET_FCPSOCKET_H

//#include <inet/common/INETDefs.h>

#include "FCPCommand_m.h"
#include <inet/networklayer/common/L3Address.h>

using namespace inet;

class FCPSocket{


protected:
    int connId;

    L3Address localAddr;
    int localPort;
    L3Address remoteAddr;
    int remotePort;

    cGate *gateToFcp;
    ICoCCTranslator * translator = nullptr;
    simtime_t startTime = 0;

    int function;
    double percentage;
    bool usePreload;

    bool enableQMSmoothing;
    int qmHistoryLength;

    simtime_t collectionInterval;
    simtime_t forcedPushDelay;
    double fcpEpsilon;
    double fcpQ;

    int metadataOverhead;
    int lowerLayerOverhead;
    int fcpOverhead;

protected:
    void sendToFCP(cMessage *msg);
    void sendTranslatorCommand();
    void sendTransportStreamStartCommand();

public:
    FCPSocket(int function, double percentage, bool usePreload, simtime_t collectionInterval, simtime_t forcedPushDelay, double fcpEpsilon,
            double fcpQ, int metadataOverhead, int lowerLayerOverhead, int fcpOverhead, bool useSmoothing, int qmHistoryLength);
    ~FCPSocket();

    int getConnectionId() const {return connId;}
    L3Address getLocalAddress() {return localAddr;}
    int getLocalPort() {return localPort;}
    L3Address getRemoteAddress() {return remoteAddr;}
    int getRemotePort() {return remotePort;}
    void setTranslator(ICoCCTranslator* trans);
    void setTransportStreamStart(simtime_t);

    void setOutputGate(cGate *toFcp) {gateToFcp = toFcp;}

    void bind(int port);

    void listen();

    void connect(L3Address rAddr, int rPort);

    void send(cMessage *msg);

    void sendCommand(cMessage *msg);

    void close();

    void processMessage(cMessage* msg);
};

#endif
