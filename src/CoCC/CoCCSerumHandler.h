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

#ifndef __LIBNCS_OMNET_COCCSERUMHANDLER_H_
#define __LIBNCS_OMNET_COCCSERUMHANDLER_H_

#include <omnetpp.h>

#include <memory>

#include <inet/networklayer/serum/SerumSupport.h>
#include "CoCCUDPTransport.h"
#include "BloomFilters.hpp"

using namespace omnetpp;
using namespace inet;

struct CoCCRecordIdentifier {
    IPv6Address srcAddress;
    short period;
};

namespace std {
    template<> struct hash<CoCCRecordIdentifier> {
        std::size_t operator()(CoCCRecordIdentifier const& key) const noexcept {
            std::hash<uint32_t> wordHash;
            std::hash<short> shortHash;

            size_t hashValue = shortHash(key.period);

            for (int i = 0; i < 4; i++) {
                hashValue = hashValue * 31 + wordHash(key.srcAddress.words()[i]);
            }

            return hashValue;
        }
    };
}

class CoCCSerumHandler : public SerumSupport::RecordHandler, public cSimpleModule {

  public:

    CoCCSerumHandler();
    virtual ~CoCCSerumHandler();
    virtual std::vector<SerumSupport::DatasetInfo> datasetDescriptors();
    virtual void* interfaceAdded(const InterfaceEntry * const ie, const short dataDesc);
    virtual void handleAppendRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex);
    virtual void handleInlineRecord(void * const handlerData, IPv6Datagram * const pkt, TLVOptions * const opts, const uint optIndex);

  protected:

    virtual void initialize();
    virtual void handleMessage(cMessage *msg);

    typedef CountingBloomFilter<CoCCRecordIdentifier> BloomFilter;

    friend class CoCCData;
    struct CoCCData {
        double m_sum_filtered = 0;
        double b_sum_filtered = 0;
        double m_sum_total = 0;
        double b_sum_total = 0;
        double qm_thresh = 0;
        double qmDesiredRate_sum = 0;

        int flows = 0;

        double curFallbackRate_sum = 0;
        double prevFallbackRate_sum = 0;
        BloomFilter curCollectedRecords;
        BloomFilter prevCollectedRecords;

        MonitoringCollector::Statistics stats;

        CoCCData(const CoCCSerumHandler * handler);
        bool add(const IPv6Address &addr, CoCCPushRecord * const &pr);
        void reset();
    };

    friend class InterfaceData;
    struct InterfaceData {
        const InterfaceEntry * const ie;
        const int interfaceId;
        CoCCData accumulator;
        double expectedRateT1 = 0, expectedRateT2 = 0; // control rate which is expected without rate control regulation in the past two periods
        double rateControlFactor = 1;

        std::unique_ptr<CoCCResponseRecord> currentRecord;

        InterfaceData(const CoCCSerumHandler * handler, const InterfaceEntry * const ie);
    };

    void concludeCollection();

    cMessage * tickerEvent;
    short period = 0;
    std::list<InterfaceData> ifaces;

    simtime_t collectionInterval;
    bool enableRobustCollection;
    long bloomFlowCount;
    double bloomErrorRate;

    double targetUtilization;
    CoCCUDPTransport::CoexistenceMode coexistenceMode;

    bool enableQueueReduction;
    double acceptableQueueUtilization;
    simtime_t queueReductionTime;

    bool enableRateControl;

    simsignal_t s_pushDelayed;
    simsignal_t s_pushDuplicate;
    simsignal_t s_mFiltered;
    simsignal_t s_bFiltered;
    simsignal_t s_mReported;
    simsignal_t s_bReported;
    simsignal_t s_beRate;
    simsignal_t s_qmRate;
    simsignal_t s_ctrlRate;
    simsignal_t s_qmThresh;
    simsignal_t s_numFlows;
};

#endif
