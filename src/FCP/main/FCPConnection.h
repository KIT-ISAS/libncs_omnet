#ifndef __LIBNCS_OMNET_FCPCONNECTION_H
#define __LIBNCS_OMNET_FCPCONNECTION_H

#include <inet/common/INETDefs.h>
#include <map>

#include <inet/networklayer/common/L3Address.h>
#include "FCP.h"
#include "FCPPacket.h"
#include "FCPSendQueue.h"
#include <inet/networklayer/contract/NetworkOptions.h>
#include <inet/networklayer/ipv6/IPv6ExtensionHeaders.h>
#include "../serum/FCPSerumHeader_m.h"

using namespace inet;

class FCPSendQueue;


//All the states a fcp connection can have
enum FcpState {
    FCP_S_INIT = 0,
    FCP_S_LISTEN = FSM_Steady(1),
    FCP_S_SYN_SENT = FSM_Steady(2),
    FCP_S_SYN_ACK_SENT = FSM_Steady(3),
    FCP_S_ESTABLISHED = FSM_Steady(4),
    FCP_S_CLOSED = FSM_Steady(6),
};

//Coding of specific events
enum FCPEventCode {
    FCP_E_IGNORE,
    FCP_E_OPEN_ACTIVE,
    FCP_E_OPEN_PASSIVE,
    FCP_E_SEND,
    FCP_E_CLOSE,
    FCP_E_RCV_DATA,
    FCP_E_RCV_SYN,
    FCP_E_RCV_SYN_ACK,
    FCP_E_RCV_ACK,
    FCP_E_RCV_FIN,
    FCP_E_TRANSLATOR,
    FCP_E_STREAMSTART
};

class FCPConnection{
    public:

      //Information about the application
      int appGateIndex = -1;
      int connId = -1;

      L3Address localAddr;
      L3Address remoteAddr;
      int localPort = -1;
      int remotePort = -1;

    protected:
      //Main fcp module
      FCP *fcpMain = nullptr;
      ICoCCTranslator* translator = nullptr;
      simtime_t startTime = 0;

      double rtt;
      double price;
      double preload;
      double budget;
      int lastAckedPacket;
      int sequenceNo;
      std::map<int, double> timePacketSent;
      std::list<double> qocHistory;
      unsigned int baseSendingRate = 100000;

      double avgBNQMToAnnounce;
      double avgBNQMToCalculate;
      double avgBNBudgetToCalculate;
      int bottleneckFlows;

      double targetQM;

      std::list<double> pushedBNQMValues;
      std::list<double> pushedQMValues;
      std::list<double> targetQMHistory;

      simtime_t collectionInterval;
      simtime_t forcedPushDelay;
      int inactivityCounter = 0;
      double fcpEpsilon;
      double fcpQ;

      int metadataOverhead;
      int lowerLayerOverhead;
      int fcpOverhead;

      cMessage * pushTicker = nullptr;
      simtime_t pushPeriodStart;


      //The current state of the connection
      cFSM state;

      FCPSendQueue *sendQueue = nullptr;

      int function;
      double percentage;
      bool usePreload;

      bool enableQMSmoothing;
      unsigned qmHistoryLength;

    protected:

      //Handle events from the application
      virtual void processOpenActive(FCPEventCode& event, FCPCommand *command, cMessage *msg);
      virtual void processOpenPassive(FCPEventCode& event, FCPCommand *command, cMessage *msg);
      virtual void processSend(FCPEventCode& event, FCPCommand *command, cMessage *msg);
      virtual void processClose(FCPEventCode& event, FCPCommand *command, cMessage *msg);
      virtual void processTranslator(FCPEventCode& event, FCPCommand *command, cMessage *msg);
      virtual void processTransportStreamStart(FCPEventCode& event, FCPCommand *command, cMessage *msg);
      virtual bool performStateTransition(const FCPEventCode& event);
      virtual FCPEventCode preanalyseAppCommandEvent(int commandCode);


      //Handle events from received packets
      virtual FCPEventCode processPacket(FCPPacket *packet, NetworkOptions* no);
      virtual FCPEventCode processPacketListen(FCPPacket *packet, L3Address srcAddr, L3Address destAddr);
      virtual FCPEventCode processPacketSynSent(FCPPacket *packet, L3Address srcAddr, L3Address destAddr);
      virtual FCPEventCode processPacketSynAckSent(FCPPacket *packet, L3Address srcAddr, L3Address destAddr);

      //Utility
      virtual FCPEventCode processData(FCPPacket *packet, NetworkOptions* no);
      virtual FCPEventCode processMetadataPacket(FCPPacket *packet, NetworkOptions* no);
      virtual void receivedAck(FCPPacket* packet, NetworkOptions* no);
      virtual void sendToApp(cMessage *msg);
      virtual void sendSyn();
      virtual void sendSynAck(FCPPacket* originalPacket);
      virtual void sendAck(FCPPacket* originalPacket, NetworkOptions* no);
      virtual void sendPacket(FCPPacket *packet, IPv6HopByHopOptionsHeader* hho);
      virtual void sendData();
      virtual void sendEstabIndicationToApp();
      virtual void updateBudget();
      virtual void updateSendingRate();
      virtual void fillAckPacket(FCPPacket* originalPacket, FCPPacket* ackPacket, NetworkOptions* no);


      virtual IPv6HopByHopOptionsHeader* createExtensionHeader(SerumRecord* sr);
      virtual FCPPushRecord* createFCPPushRecord(int size, bool coolectQM);
      virtual FCPResponseRecord* createFCPResponseRecord();

      virtual bool pushMetadata(IPv6HopByHopOptionsHeader* &hho, int size);

      virtual void addQMValue(double qm);
      virtual double getAverageQM();


    public:
      // handles the push ticker message from FCP.cc
      virtual void handlePushTickerEvent();

      virtual bool processCommandFromApp(cMessage *msg);
      virtual bool processFCPPacket(FCPPacket *packet, L3Address srcAddr, L3Address destAddr, NetworkOptions* no);

      virtual void sendFin();
      virtual void sendToIp(FCPPacket *packet, IPv6HopByHopOptionsHeader* hho);
      virtual FCPPacket *createFCPPacket(const char *name);

      FCPConnection(FCP *main, int appGateIndex, int connId);
      virtual ~FCPConnection();

};

#endif
