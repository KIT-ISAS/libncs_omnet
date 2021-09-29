#include <algorithm>
#include <string>
#include "FCP.h"
#include "FCPPacket.h"
#include "FCPConnection.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/IPSocket.h"
#include "inet/networklayer/contract/generic/GenericNetworkProtocolControlInfo.h"
#include "inet/networklayer/contract/IL3AddressType.h"
#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/contract/ipv4/IPv4ControlInfo.h"
#include "inet/networklayer/contract/ipv6/IPv6ControlInfo.h"
#include "inet/networklayer/ipv6/IPv6ExtensionHeaders.h"


Define_Module(FCP);

#define EPHEMERAL_PORTRANGE_START   1024
#define EPHEMERAL_PORTRANGE_END     5000
#define AVAILABLE_BALANCE           100
#define FCP_TICKER_MSG_KIND 1337


FCP::~FCP(){
    for (auto iterator = fcpAppConnectionMap.begin(); iterator != fcpAppConnectionMap.end();){
        delete iterator->second;

        iterator = fcpAppConnectionMap.erase(iterator);
    }
}

/*
 * Initialize of a new FCP module.
 */
void FCP::initialize(int stage){
    cSimpleModule::initialize(stage);

    lastEphemeralPort = EPHEMERAL_PORTRANGE_START;

    rateSignal = registerSignal("rate");
    priceEndpSignal = registerSignal("priceEndp");
    budgetSignal = registerSignal("budget");
    targetBudgetSignal = registerSignal("targetBudget");
    averageQocSignal = registerSignal("averageQoc");

    avgBNQMToCalcSignal = registerSignal("avgBNQMToCalc");
    avgBNQMToAnnounceSignal = registerSignal("avgBNQMToAnnounce");
    avgBNBudgetSignal = registerSignal("avgBNBudget");

    avgTargetQMSignal = registerSignal("avgTargetQM");

    numBNFlowsSignal = registerSignal("numBNFlows");
    changedBudgetSignal = registerSignal("changedBudget");
    qmDiffSignal = registerSignal("qmDiff");

    if(stage == INITSTAGE_TRANSPORT_LAYER){
        IPSocket ipSocket(gate("ipOut"));
        ipSocket.registerProtocol(IP_PROT_FCP);
    }
}

/*
 * Just a basic finish method.
 */
void FCP::finish(){
    EV_INFO << getFullPath() << ": finishing with " << fcpAppConnectionMap.size() << " connections open.\n";
}

/*
 * Creates a new connection for an application
 */
FCPConnection *FCP::createConnection(int appGateIndex, int connId){
    FCPConnection *connection = new FCPConnection(this, appGateIndex, connId);
    fcpAppConnectionMap[connId] = connection;
    return connection;

}

/*
 * Closes a connection and deletes it from the connection maps.
 */
void FCP::close(FCPConnection *connection){
    int connId = connection->connId;
    fcpAppConnectionMap.erase(connId);

    SocketPair socket;
    socket.localAddr = connection->localAddr;
    socket.remoteAddr = connection->remoteAddr;
    socket.localPort = connection->localPort;
    socket.remotePort = connection->remotePort;
    fcpSocketConnectionMap.erase(socket);

    auto iterator = usedEphemeralPorts.find(connection->localPort);

    if (iterator != usedEphemeralPorts.end())
        usedEphemeralPorts.erase(iterator);

    delete connection;
}

/*
 * Finds a connection for an application and the corresponding connection ID
 */
FCPConnection *FCP::findConnectionForApp(int appGateIndex, int connId){
    auto iterator = fcpAppConnectionMap.find(connId);
    return iterator == fcpAppConnectionMap.end() ? nullptr : iterator->second;
}

/*
 * Finds a fitting connection for an arriving packet
 */
FCPConnection *FCP::findConnectionForPacket(FCPPacket *packet, L3Address srcAddr, L3Address destAddr){
    SocketPair socket;
    socket.localAddr = destAddr;
    socket.remoteAddr = srcAddr;
    socket.localPort = packet->getDestPort();
    socket.remotePort = packet->getSrcPort();

    auto iterator = fcpSocketConnectionMap.find(socket);
    if(iterator != fcpSocketConnectionMap.end()){
        return iterator->second;
    }

    /*
     * Try to find a connection with some missing details.
     */
    socket.localAddr = L3Address();
    iterator = fcpSocketConnectionMap.find(socket);
    if(iterator != fcpSocketConnectionMap.end()){
        return iterator->second;
    }

    socket.localAddr = destAddr;
    socket.remoteAddr = L3Address();
    socket.remotePort = -1;
    iterator = fcpSocketConnectionMap.find(socket);
    if(iterator != fcpSocketConnectionMap.end()){
        return iterator->second;
    }

    socket.localAddr = L3Address();
    iterator = fcpSocketConnectionMap.find(socket);
    if(iterator != fcpSocketConnectionMap.end()){
        return iterator->second;
    }

    return nullptr;
}

/*
 * Handles incoming messages and sends them to the according method.
 */
void FCP::handleMessage(cMessage *msg) {

    if(msg->arrivedOn("ipIn")){
        //Packet came from IP and gets sent to the application
        FCPPacket *packet = check_and_cast<FCPPacket *>(msg);

        L3Address srcAddr;
        L3Address destAddr;
        NetworkOptions no;

        cObject *ctrl = packet->removeControlInfo();

        if (dynamic_cast<IPv4ControlInfo *>(ctrl) != nullptr){
            IPv4ControlInfo *ctrl4 = (IPv4ControlInfo *)ctrl;
            srcAddr = ctrl4->getSrcAddr();
            destAddr = ctrl4->getDestAddr();
        } else if(dynamic_cast<IPv6ControlInfo *>(ctrl) != nullptr){
            IPv6ControlInfo *ctrl6 = (IPv6ControlInfo *)ctrl;
            srcAddr = ctrl6->getSrcAddr();
            destAddr = ctrl6->getDestAddr();

            if (ctrl6->getExtensionHeaderArraySize() > 0) {
                while (ctrl6->getExtensionHeaderArraySize() > 0) {
                    IPv6ExtensionHeader * const eh = ctrl6->removeFirstExtensionHeader();

                    ASSERT(eh);

                    if (dynamic_cast<IPv6HopByHopOptionsHeader *>(eh) != nullptr){
                        no.addV6Header((IPv6HopByHopOptionsHeader *)eh);
                    }
                }
            }
        }

        delete ctrl;

        FCPConnection *connection = findConnectionForPacket(packet, srcAddr, destAddr);

        if(connection){
            bool a = connection->processFCPPacket(packet, srcAddr, destAddr, &no);

            if(!a){
                close(connection);
            }
        } else{
            throw cRuntimeError("Error: No connection found for the message.");
        }

    } else if (msg->isSelfMessage()) {
        // Ticker event

        switch (msg->getKind()) {
        case FCP_TICKER_MSG_KIND: {

            EV_DETAIL << "Handle FCP_TICKER_MSG_KIND" << endl;

            FCPConnection* const connection = (FCPConnection*) (msg->getContextPointer());
            connection->handlePushTickerEvent();
        } break;
        default:
            const char * const name = msg->getName();

            delete msg;

            throw cRuntimeError("Received unexpected message: %s", name);
        }


    } else{
        //Packet came from the application and gets sent to IP
        FCPCommand *controlInfo = check_and_cast<FCPCommand *>(msg->getControlInfo());
        int appGateIndex = msg->getArrivalGate()->getIndex();
        int connId = controlInfo->getConnId();

        FCPConnection *conn = findConnectionForApp(appGateIndex, connId);

        if (!conn) {
            conn = createConnection(appGateIndex, connId);
        }
        bool ret = conn->processCommandFromApp(msg);
        if (!ret)
            close(conn);
    }
}

/**
 * Adds a new socket with a corresponding connection to the SocketConnectionMap
 */
void FCP::addSocketPair(FCPConnection *connection, L3Address localAddr, L3Address remoteAddr, int localPort, int remotePort){
    SocketPair socket;
    socket.localAddr = connection->localAddr = localAddr;
    socket.remoteAddr = connection->remoteAddr = remoteAddr;
    socket.localPort = connection->localPort = localPort;
    socket.remotePort = connection->remotePort = remotePort;

    auto iterator = fcpSocketConnectionMap.find(socket);
    if(iterator != fcpSocketConnectionMap.end()){
        throw cRuntimeError("Error: There is already a connection like this.");
    }

    fcpSocketConnectionMap[socket] = connection;

    if (localPort >= EPHEMERAL_PORTRANGE_START && localPort < EPHEMERAL_PORTRANGE_END){
        usedEphemeralPorts.insert(localPort);
    }
}

/*
 * Changes an existing socket pair.
 */
void FCP::updateSocketPair(FCPConnection *connection, L3Address localAddr, L3Address remoteAddr, int localPort, int remotePort){
    //Find old socket
    SocketPair socket;
    socket.localAddr = connection->localAddr;
    socket.remoteAddr = connection->remoteAddr;
    socket.localPort = connection->localPort;
    socket.remotePort = connection->remotePort;

    auto iterator = fcpSocketConnectionMap.find(socket);
    fcpSocketConnectionMap.erase(iterator);

    //Change details of the old socket, to fit the new information
    socket.localAddr = connection->localAddr = localAddr;
    socket.remoteAddr = connection->remoteAddr = remoteAddr;
    socket.remotePort = connection->remotePort = remotePort;

    fcpSocketConnectionMap[socket] = connection;

    EV_DETAIL << "Updating socket to: srcAddr: " << localAddr << " destAddr: " << remoteAddr << " srcPort: " << localPort << " destPort: " << remotePort << "\n";
}

/*
 * Find a port that isn't already in use for an open command, which doesn't specify one.
 */
ushort FCP::getEphemeralPort(){
    ushort searchUntil = lastEphemeralPort++;
    if (lastEphemeralPort == EPHEMERAL_PORTRANGE_END){
        lastEphemeralPort = EPHEMERAL_PORTRANGE_START;
    }

    while (usedEphemeralPorts.find(lastEphemeralPort) != usedEphemeralPorts.end()) {
        if (lastEphemeralPort == searchUntil)
            throw cRuntimeError("Error: All ephemeral ports in use.");

        lastEphemeralPort++;

        if (lastEphemeralPort == EPHEMERAL_PORTRANGE_END)
            lastEphemeralPort = EPHEMERAL_PORTRANGE_START;
    }

    return lastEphemeralPort;
}
