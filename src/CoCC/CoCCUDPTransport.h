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

#include <memory>

#include <inet/networklayer/serum/SerumSupport.h>
#include <inet/transportlayer/contract/udp/UDPSocket.h>

#include "CoCCTranslator.h"
#include "CoCCSerumHeader_m.h"
#include "util/UDPHandshakePkt_m.h"
#include "util/TransportCtrlMsg.h"
#include "MockImpl/util/WindowStats.h"

using namespace omnetpp;
using namespace inet;


#define COCC_EPSILON 0.0001
#define COCC_QM_MIN 0.0
#define COCC_QM_MAX 1.0
#define CLAMP(clamp_x, clamp_min, clamp_max) std::min(std::max((clamp_x), (clamp_min)), (clamp_max))


class CoCCUDPTransport : public cSimpleModule, public ICoCCTranslator::IControlObserver {
  public:
    CoCCUDPTransport();
    virtual ~CoCCUDPTransport();

    enum CoexistenceMode {
        CM_DISABLED = 0,
        CM_COOPERATIVE,
        CM_REACTIVE,
        CM_SUBMISSION,
        CM_TOTAL_SUBMISSION
    };

  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage * const msg);

  protected:
    // types
    struct PeriodRecord {
        bool valid = false;
        bool pushSent = false;
        bool responseReceived = false;

        short period;
        double bottleneckQM;
        double bottleneckQMThresh;
        ICoCCTranslator::CoCCLinearization lin;
        double targetQM;
        double expectedBitRate;
    };
    struct PushData {
        short period;
        bool sent = false;                      // true once a push record was sent

        ICoCCTranslator::CoCCLinearization lin; // linearization at bottleneckQM
        double bottleneckQM;                    // targetQM for bottleneck link in last period
    };
    struct FeedbackData {
        short period;
        bool valid = false;   // true once feedback was received and processed

        double bottleneckQM;  // targetQM for bottleneck link
        double targetQM;      // with local adjustments
        double targetBitRate; // bitrate for targetQM
    };
    struct SocketHandle_t {
        std::shared_ptr<UDPSocket> socket;

        bool listening;
        bool connected;
        uint16_t port;
        L3Address remote;

        // CoCC
        NcsContextComponentIndex role = NCTXCI_COUNT; // invalid
        ICoCCTranslator * translator = nullptr;
        cMessage * streamStartEvent = nullptr;
        cMessage * streamStopEvent = nullptr;
        cMessage * pushTicker = nullptr;
        cMessage * targetCommitEvent = nullptr;
        simtime_t collectionPeriodStart;
        simtime_t pushStart;
        int pushWaitCounter;
        simtime_t forcedPushTime;
        double effectiveForcedPushFraction;
        int inactivityCounter = 0;
        simtime_t stopTime = SIMTIME_ZERO;
        std::unique_ptr<IPv6ExtensionHeader> pendingRequest;

        short pushPeriod;
        PushData push;
        FeedbackData feedback;

        long burstCredit;
        double expectedBitRate;
        simtime_t lastCreditUpdate;

        double lbeClassAccumulator = 0;

        RunningWindowStats<double> forcedPushHistory;
        RunningWindowStats<double> expectedPushHistory;

        ~SocketHandle_t();
    };
    typedef std::map<int, SocketHandle_t *> SocketMap_t;
    typedef std::vector<SocketHandle_t *> SocketVector_t;

    // variables
    cGate *udpIn;
    cGate *udpOut;
    cGate *upIn;
    cGate *upOut;

    simsignal_t s_expectedRate;
    simsignal_t s_rateLimitDropSignal;
    simsignal_t s_reportedM;
    simsignal_t s_reportedB;
    simsignal_t s_reportedQMtarget;
    simsignal_t s_lastFallbackRate;
    simsignal_t s_qmDesiredRate;
    simsignal_t s_periodMismatch;
    simsignal_t s_responseMissed;
    simsignal_t s_bottleneckIndex;
    simsignal_t s_bottleneckCtrlRate;
    simsignal_t s_bottleneckShare;
    simsignal_t s_bottleneckQM;
    simsignal_t s_appliedQM;
    simsignal_t s_pushForced;
    simsignal_t s_forcedPushCompensation;

    SocketMap_t connectionMap; // ConnId --> SocketHandle_t;
    SocketVector_t connectionVect;
    SocketHandle_t * listenSocket = nullptr;

    simtime_t collectionInterval;
    bool enableRobustCollection;
    bool enablePushSpreading;
    bool autoSpreading;
    double ackFraction;
    simtime_t maxPushPathDelay;
    double regularPushFraction;
    double forcedPushFraction;

    bool continousQMAdjustment;
    bool compensateLinearizationErrors;
    long pushCompensationHorizon;

    bool enableRateLimiting;
    int lowerLayerOverhead;
    int metadataOverhead;
    int permittedBurstSize;

    CoexistenceMode coexistenceMode;
    double qmDesired;

    // inout processing
    TransportDataInfo * createTransportInfo(UDPDataIndication * const ctrl);
    void handleIncomingHandshake(UDPHandshake * const hs, UDPDataIndication * const ctrl,
            SocketHandle_t * const handle);
    void processConnectRequest(TransportConnectReq* const req);
    void processListenRequest(const uint16_t listenPort);
    void handleConnectTimeout(cMessage* const msg);

    // CoCC
    void coccProcessMonitoringRequest(SocketHandle_t * const handle, TransportDataInfo * const info);
    void coccProcessFeedback(SocketHandle_t * const handle, TransportDataInfo * const info);
    void coccHandleStreamStart(cMessage * const msg, simtime_t start);
    void coccHandleStreamStart(SocketHandle_t * const handle, simtime_t start);
    void coccHandleStreamStop(cMessage * const msg, simtime_t stop);
    void coccHandleStreamStop(SocketHandle_t * const handle, simtime_t stop);
    void coccSetTranslator(SocketHandle_t* const handle, ICoCCTranslator* const translator, const NcsContextComponentIndex role);
    bool coccReplyToMonitoringRequest(SocketHandle_t * const handle, NetworkOptions* &opts);
    void coccSchedulePushTicker(SocketHandle_t * const handle);
    bool coccPushMetadata(SocketHandle_t * const handle, NetworkOptions* &opts);
    double coccEstimateForcedPushLikelyhood(SocketHandle_t * const handle);
    void coccCoexistenceHandler(SocketHandle_t * const handle, NetworkOptions* &opts);
    void coccHandlePushTicker(cMessage * const msg);
    void coccHandleDrivingPushTicker(SocketHandle_t * const handle);
    void coccHandleRespondingPushTicker(SocketHandle_t * const handle);
    void coccHandleCommitTicker(cMessage * const msg);
    bool coccRateLimitAccept(SocketHandle_t * const handle, cPacket * const pkt);

  public:
    virtual void postControlStep(void * const context) override;

  protected:
    // socket management methods
    SocketHandle_t* createSocket();
    void connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port);
    void storeSocket(SocketHandle_t * const handle);
    void initHandshake(SocketHandle_t* const handle, cMessage* const selfMsg);
    SocketHandle_t * getSocketById(const int id);
    SocketHandle_t * getSocketByAddr(const L3Address& addr, const uint16_t port);

  public:
    static double coccComputeCoexistenceRate(const CoCCUDPTransport::CoexistenceMode coexistenceMode, const double availableRate,
            const double qmDesiredRate, const double beRate);
    static double coccComputeQueueReduction(const double avgQueueLength, const double avgQueuePktBits,
            const double acceptableQueueUtilization, const simtime_t queueReductionTime);
    static double coccComputeLinkTargetQM(const double targetRate, const double mSum, const double bSum, const bool clamp = true);
};

#endif
