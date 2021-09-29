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

#ifndef __LIBNCS_OMNET_UDPTRANSPORT_H_
#define __LIBNCS_OMNET_UDPTRANSPORT_H_

#include <omnetpp.h>
#include <CoCC/CoCCTranslator.h>

#include <memory>

#include <inet/networklayer/serum/SerumSupport.h>
#include <inet/transportlayer/contract/udp/UDPSocket.h>

#include "simpleCCSerumHeader_m.h"
#include "util/UDPHandshakePkt_m.h"
#include "util/TransportCtrlMsg.h"

using namespace omnetpp;
using namespace inet;


#define simpleCC_EPSILON 0.0001
#define simpleCC_QM_MIN 0.0
#define simpleCC_QM_MAX 1.0
#define CLAMP(clamp_x, clamp_min, clamp_max) std::min(std::max((clamp_x), (clamp_min)), (clamp_max))



class simpleCCUDPTransport : public cSimpleModule {
  public:
    simpleCCUDPTransport();
    virtual ~simpleCCUDPTransport();

  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

  protected:
    // types
    struct SocketHandle_t {
        std::shared_ptr<UDPSocket> socket;


        bool listening;
        bool connected;
        uint16_t port;
        L3Address remote;

        ICoCCTranslator * translator = nullptr;
        cMessage * pushTicker = nullptr;
        simtime_t pushPeriodStart;
        simtime_t forcedPushTime;
        int inactivityCounter = 0;
        std::unique_ptr<IPv6ExtensionHeader> pendingRequest;



        // memorize connection-specific data here

        ~SocketHandle_t();
    };
    typedef std::map<int, SocketHandle_t *> SocketMap_t;
    typedef std::vector<SocketHandle_t *> SocketVector_t;

    // variables
    cGate *udpIn;
    cGate *udpOut;
    cGate *upIn;
    cGate *upOut;

    simsignal_t expectedRate;

    SocketMap_t connectionMap; // ConnId --> SocketHandle_t;
    SocketVector_t connectionVect;
    SocketHandle_t * listenSocket = nullptr;

    bool forceMonitoringReply;

    simtime_t collectionInterval;
    simtime_t pushSpread;
    simtime_t forcedPushDelay;
    simtime_t forcedPushSpread;

    int lowerLayerOverhead;
    int metadataOverhead;

    // new simpleCC


    // parameters from config
    double simpleCC_QUEUE_THRESHOLD;
    double simpleCC_CONGESTION_THRESHOLD;
    double simpleCC_FULL_LOAD_THRESHOLD;
    double simpleCC_HEAVY_CONGESTION_MULT;
    double  simpleCC_LIGHT_CONGESTION_MULT;
    double simpleCC_SLOW_START_THRESHOLD_MULT;
    bool simpleCC_enable_random_fullLoadIncrease;
    bool simpleCC_enable_dynamic_maxDrop;
    bool simpleCC_enable_weigthed_slowStartTH;
    double simpleCC_RANDOM_MIN_QUEUE_THRESHOLD;
    double simpleCC_RANDOM_MAX_QUEUE_THRESHOLD;
    double simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING;
    double simpleCC_QM_STEP;
    // other global variables
    double maxQueueLength = 0;
    double slowStartThreshold = 1.0;
    bool congestionBlocked = false;
    double lastCongestionQm = 1.0;
    double elapsedCycles = 0;
    double simpleCC_c;
    double simpleCC_beta;
    double maxTqmStep = 0.1;
    double lastCongestionMin = 0;
    double slowStartFactor = 0;

    // new simpleCC end

    // inout processing
    TransportDataInfo * createTransportInfo(UDPDataIndication * const ctrl);
    void handleIncomingHandshake(UDPHandshake * const hs, UDPDataIndication * const ctrl,
            SocketHandle_t * const handle);
    void processConnectRequest(TransportConnectReq* const req);
    void processListenRequest(const uint16_t listenPort);
    void handleConnectTimeout(cMessage* const msg);

    // simpleCC
    void simpleCCProcessMonitoringRequest(SocketHandle_t * const handle, TransportDataInfo * const info);
    void simpleCCProcessFeedback(SocketHandle_t * const handle, TransportDataInfo * const info);

    void simpleCCProcessFeedbackCubic(SocketHandle_t * const handle, TransportDataInfo * const info);

    void simpleCCSetTranslator(SocketHandle_t* const handle, ICoCCTranslator* const translator);
    void simpleCCReplyToMonitoringRequest(SocketHandle_t * const handle, NetworkOptions* &opts);
    void simpleCCSchedulePushTicker(SocketHandle_t * const handle);
    bool simpleCCPushMetadata(SocketHandle_t * const handle, NetworkOptions* &opts);
    void simpleCCHandlePushTicker(cMessage * const msg);
    void simpleCCHandlePushTicker(SocketHandle_t * const handle);


    // socket management methods
    SocketHandle_t* createSocket();
    void connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port);
    void storeSocket(SocketHandle_t * const handle);
    void initHandshake(SocketHandle_t* const handle, cMessage* const selfMsg);
    SocketHandle_t * getSocketById(const int id);
    SocketHandle_t * getSocketByAddr(const L3Address& addr, const uint16_t port);
};

#endif
