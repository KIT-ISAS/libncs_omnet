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

#ifndef __LIBNCS_OMNET_ORACLECCUDPTRANSPORT_H_
#define __LIBNCS_OMNET_ORACLECCUDPTRANSPORT_H_

#include <omnetpp.h>

#include <memory>

#include <inet/networklayer/serum/SerumSupport.h>

#include <CoCC/CoCCTranslator.h>
#include <CoCC/CoCCUDPTransport.h>
#include <OracleCC/OracleCCCoordinator.h>

#include "util/UDPHandshakePkt_m.h"
#include "util/TransportCtrlMsg.h"

using namespace omnetpp;
using namespace inet;

// forward declaration to break loop
class OracleCCCoordinator;

class OracleCCUDPTransport : public cSimpleModule, public ICoCCTranslator::IControlObserver {
  public:
    virtual ~OracleCCUDPTransport();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage * const msg) override;

  protected:
    struct SocketHandle_t {
        std::shared_ptr<UDPSocket> socket;

        bool listening;
        bool connected;
        uint16_t port;
        L3Address remote;

        // CoCC
        ICoCCTranslator * translator = nullptr;
        cMessage * streamStartEvent = nullptr;
        cMessage * streamStopEvent = nullptr;
        int inactivityCounter = 0;

        double lbeClassAccumulator = 0;

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
    simsignal_t appliedQM;

    SocketMap_t connectionMap; // ConnId --> SocketHandle_t;
    SocketVector_t connectionVect;
    SocketHandle_t * listenSocket = nullptr;

    int lowerLayerOverhead;

    CoCCUDPTransport::CoexistenceMode coexistenceMode;
    double qmDesired;

    // inout processing
    TransportDataInfo * createTransportInfo(UDPDataIndication * const ctrl);
    void handleIncomingHandshake(UDPHandshake * const hs, UDPDataIndication * const ctrl,
            SocketHandle_t * const handle);
    void processConnectRequest(TransportConnectReq* const req);
    void processListenRequest(const uint16_t listenPort);
    void handleConnectTimeout(cMessage* const msg);

    void occHandleStreamStart(cMessage * const msg, simtime_t start);
    void occHandleStreamStart(SocketHandle_t * const handle, simtime_t start);
    void occHandleStreamStop(cMessage * const msg, simtime_t stop);
    void occHandleStreamStop(SocketHandle_t * const handle, simtime_t stop);
    void occSetTranslator(SocketHandle_t* const handle, ICoCCTranslator* const translator);
    void occCoexistenceHandler(SocketHandle_t * const handle, NetworkOptions* &opts);

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
    // OracleCC coordinator access methods
    double getQMDesiredRate(void * const handle);
    double getRateForQM(void * const handle, const double qmTarget);
    ICoCCTranslator::CoCCLinearization getLinearizationForQM(void * const handle, const double qmTarget);

  private:
    OracleCCCoordinator * coord;
};

#endif
