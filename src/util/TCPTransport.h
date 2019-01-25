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

#ifndef __LIBNCS_OMNET_TCPTRANSPORT_H_
#define __LIBNCS_OMNET_TCPTRANSPORT_H_

#include <omnetpp.h>

#include <inet/transportlayer/contract/tcp/TCPSocket.h>
#include <inet/common/ByteArray.h>

#include "TransportCtrlMsg_m.h"

using namespace omnetpp;
using namespace inet;

class TCPTransport : public cSimpleModule, public TCPSocket::CallbackInterface {
  public:
    TCPTransport();
    virtual ~TCPTransport();

  public:
    virtual void socketDataArrived(const int connId, void * const yourPtr, cPacket * const msg, const bool urgent);
    virtual void socketEstablished(const int connId, void *const yourPtr);
    virtual void socketPeerClosed(const int connId, void * const yourPtr);
    virtual void socketClosed(const int connId, void * const yourPtr);
    virtual void socketFailure(const int connId, void * const yourPtr, const int code);
    virtual void socketStatusArrived(const int connId, void *const yourPtr, TCPStatusInfo * const status);
    virtual void socketDeleted(const int connId, void * const yourPtr);

  protected:
    virtual void initialize();
    virtual void handleMessage(cMessage * msg);

  protected:
    /*
     * types
     */
    // type used to represent/store the size of a data chunk, if in
    // datagramService mode.
    // Bigger chunks than max(PayloadSize_t) - sizeof(PayloadSize_t) cause TCPTransport to fail.
    typedef uint32_t PayloadSize_t;
    struct SocketHandle_t {
        TCPSocket socket;
        ByteArray buffer; // reassembly buffer
    };
    typedef std::map<int, SocketHandle_t *> SocketMap_t;

    // params
    bool datagramService;

    // variables
    cGate *tcpIn;
    cGate *tcpOut;
    cGate *upIn;
    cGate *upOut;

    bool listening = false;
    SocketMap_t connectionMap; // ConnId --> SocketHandle_t;

    // socket management methods
    SocketHandle_t* createSocket(cMessage * const msg = nullptr);
    void connectSocket(SocketHandle_t * const handle, const L3Address endpAddr, const uint16_t port);
    void storeSocket(SocketHandle_t * const handle);
    SocketHandle_t * getSocketById(const int id);
    SocketHandle_t * getSocketByAddr(const L3Address& addr);
    void deleteSocket(SocketHandle_t * const handle);

    TransportDataInfo * createDataInfo(SocketHandle_t* const handle);
};

#endif
