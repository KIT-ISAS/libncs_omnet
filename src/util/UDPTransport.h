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

#include <inet/transportlayer/contract/udp/UDPSocket.h>

#include "TransportCtrlMsg_m.h"

using namespace omnetpp;
using namespace inet;

class UDPTransport : public cSimpleModule {
  public:
    UDPTransport();
    virtual ~UDPTransport();

  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

  protected:
    // types
    struct SocketHandle_t {
        UDPSocket * socket;

        bool listening;
        bool connected;
        uint16_t port;
        L3Address remote;
    };
    typedef std::map<int, SocketHandle_t *> SocketMap_t;
    typedef std::vector<SocketHandle_t *> SocketVector_t;

    // variables
    cGate *udpIn;
    cGate *udpOut;
    cGate *upIn;
    cGate *upOut;

    SocketMap_t connectionMap; // ConnId --> SocketHandle_t;
    SocketVector_t connectionVect;
    SocketHandle_t * listenSocket = nullptr;

    // socket management methods
    SocketHandle_t* createSocket();
    void connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port);
    void storeSocket(SocketHandle_t * const handle);
    void initHandshake(SocketHandle_t* const handle, cMessage* const selfMsg);
    SocketHandle_t * getSocketById(const int id);
    SocketHandle_t * getSocketByAddr(const L3Address& addr, const uint16_t port);
};

#endif
