#ifndef __LIBNCS_OMNET_FCPTRANSPORT_H_
#define __LIBNCS_OMNET_FCPTRANSPORT_H_

#include <omnetpp.h>

#include "FCP/contract/FCPSocket.h"
#include "TransportCtrlMsg.h"

using namespace omnetpp;

class FCPTransport : public cSimpleModule {
protected:
    typedef std::map<int, FCPSocket *> SocketMap_t;

    cGate *fcpIn;
    cGate *fcpOut;
    cGate *upIn;
    cGate *upOut;

    SocketMap_t connectionMap;

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

public:
    FCPTransport();
    virtual ~FCPTransport();

protected:
    virtual void initialize();
    virtual void handleMessage(cMessage * msg);

    FCPSocket* createSocket();
    void connectSocket(FCPSocket * handle, L3Address endpAddr, uint16_t port);
    void storeSocket(FCPSocket * handle);
    FCPSocket * getSocketById(int id);
    FCPSocket * getSocketByAddr(L3Address& addr);
    void deleteSocket(FCPSocket * handle);
    TransportDataInfo* createTransportInfo(FCPSocket* socket);
};

#endif
