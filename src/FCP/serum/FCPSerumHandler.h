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

#ifndef __LIBNCS_OMNET_FCPSERUMHANDLER_H_
#define __LIBNCS_OMNET_FCPSERUMHANDLER_H_

#include <omnetpp.h>

#include <deque>

#include <inet/networklayer/serum/SerumSupport.h>

using namespace omnetpp;
using namespace inet;

class FCPSerumHandler : public SerumSupport::RecordHandler, public cSimpleModule {

  public:

    FCPSerumHandler();
    virtual ~FCPSerumHandler();
    virtual std::vector<SerumSupport::DatasetInfo> datasetDescriptors();
    virtual void* interfaceAdded(const InterfaceEntry * const ie, const short dataDesc);
    virtual void handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex);
    virtual void handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex);

  protected:

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    struct PacketData {
        simtime_t arrival;
        double matchedPrice;
        double calculatedPrice;
        double value;

        PacketData(simtime_t t, double matchedP, double calcP, double v);
    };

    struct FCPQMData {
        double budget_sum;
        double qm_sum;
        double averageBudget;
        double averageQM;
        int flows;
        double scaledQM;
        double originalQM;

        FCPQMData();
        void add(const double q, const double b);
        void calculateAverage(double util, double utilThr);
        void reset();
    };

    struct FCPData {
        std::list<PacketData> packets;

        FCPData();
        void addPacket(simtime_t t, double matchedPrice, double calculatedPrice, double value);
        int removePackets(simtime_t t, simtime_t window);
        double findMatchingPrice(simtime_t t, double rtt);
        double getSum();
    };

    struct InterfaceData {
        const InterfaceEntry * const ie;
        const int interfaceId;

        FCPData lastPackets;
        FCPQMData qmAccumulator;
        FCPQMData qmCollected;
        std::deque<double> collectedQMHistory;
        std::list<int> flowsHistory;

        std::list<double> itHistory;

        double utilization = 0;

        InterfaceData(const InterfaceEntry * const ie);
        void concludeCollection(double utilThr);
        void addItValue(double it, int historyLength);
        double getAverageIt();
    };

    simtime_t collectionInterval;
    simtime_t averagingWindow;
    double alpha;

    double utilizationThreshold;

    double minPrice;

    bool enableItSmoothing;
    int itHistoryLength;

    cMessage * tickerEvent;
    std::list<InterfaceData> ifaces;

    simsignal_t priceSignal;
    simsignal_t i_tSignal;
    simsignal_t sumSignal;

    simsignal_t rttSignal;
    simsignal_t matchingPriceSignal;
    simsignal_t packetSizeSignal;
    simsignal_t packetValueSignal;
    simsignal_t remainingLinkCapacitySignal;
    simsignal_t avgQueueLengthSignal;

    simsignal_t lastPacketsSizeSignal;

    simsignal_t avgQMSignal;
    simsignal_t avgBudgetSignal;
    simsignal_t utilizationSignal;

    simsignal_t packetArrivedSignal;
    simsignal_t oldestPacketDiffSignal;

    simsignal_t originalQMSignal;
    simsignal_t scaledQMSignal;
};

#endif
