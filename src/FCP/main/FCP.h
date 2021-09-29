#ifndef __LIBNCS_OMNET_FCP_H
#define __LIBNCS_OMNET_FCP_H

#include <map>

#include <inet/common/INETDefs.h>

#include <inet/networklayer/common/L3Address.h>
#include "FCP/contract/FCPCommand_m.h"

class FCPPacket;
class FCPConnection;

using namespace inet;
using namespace omnetpp;

class INET_API FCP : public cSimpleModule{

    public:

    struct SocketPair
    {
        L3Address localAddr;
        L3Address remoteAddr;
        int localPort;
        int remotePort;

        inline bool operator<(const SocketPair& b) const
        {
            if (remoteAddr != b.remoteAddr)
                return remoteAddr < b.remoteAddr;
            else if (localAddr != b.localAddr)
                return localAddr < b.localAddr;
            else if (remotePort != b.remotePort)
                return remotePort < b.remotePort;
            else
                return localPort < b.localPort;
        }
    };

    simsignal_t rateSignal;
    simsignal_t priceEndpSignal;
    simsignal_t budgetSignal;
    simsignal_t targetBudgetSignal;
    simsignal_t averageQocSignal;

    simsignal_t avgBNQMToCalcSignal;
    simsignal_t avgBNQMToAnnounceSignal;
    simsignal_t avgBNBudgetSignal;

    simsignal_t avgTargetQMSignal;

    simsignal_t numBNFlowsSignal;
    simsignal_t changedBudgetSignal;
    simsignal_t qmDiffSignal;


    protected:
        typedef std::map<int, FCPConnection *> FcpAppConnMap;
        typedef std::map<SocketPair, FCPConnection *> FcpSocketConnMap;
        FcpAppConnMap fcpAppConnectionMap;
        FcpSocketConnMap fcpSocketConnectionMap;

        ushort lastEphemeralPort = (ushort)-1;
        std::multiset<ushort> usedEphemeralPorts;



    protected:


        virtual void initialize(int stage) override;
        virtual int numInitStages() const override { return NUM_INIT_STAGES; }
        virtual void finish() override;

        //Message handling
        virtual void handleMessage(cMessage *msg) override;

        //Connection handling
        virtual FCPConnection *findConnectionForPacket(FCPPacket *packet, L3Address srcAddr, L3Address destAddr);
        virtual FCPConnection *findConnectionForApp(int appGateIndex, int connId);
        virtual FCPConnection *createConnection(int appGateIndex, int connId);

    public:
        FCP(){}
        virtual ~FCP();

        virtual void close(FCPConnection *connection);

        virtual void addSocketPair(FCPConnection *connection, L3Address localAddr, L3Address remoteAddr, int localPort, int remotePort);
        virtual void updateSocketPair(FCPConnection *connection, L3Address localAddr, L3Address remoteAddr, int localPort, int remotePort);

        virtual ushort getEphemeralPort();
};
#endif
