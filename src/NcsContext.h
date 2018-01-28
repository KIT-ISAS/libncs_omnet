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

#ifndef __LIBNCS_OMNET_NCSCONTEXT_H_
#define __LIBNCS_OMNET_NCSCONTEXT_H_

#include <omnetpp.h>
#include <inet/common/RawPacket.h>
#include <inet/networklayer/common/L3Address.h>
#include <libncs_matlab.h>

using namespace omnetpp;
using namespace inet;


enum NcsContextMessageKind {
    NCTXMK_TICKER_EVT = 2300
};

enum NcsContextComponentIndex {
    NCTXCI_ACTUATOR = 0,
    NCTXCI_CONTROLLER,
    NCTXCI_SENSOR,
    NCTXCI_COUNT
};

class NcsContext : public cSimpleModule {
  public:
    virtual ~NcsContext() { };

  protected:
    virtual int numInitStages() const;
    virtual void initialize(const int stage);
    virtual void finish();
    virtual void handleMessage(cMessage * const msg);

    virtual std::vector<const char *> getConfigFieldNames();
    virtual void setConfigValues(mwArray &cfgStruct);

    void testNonnegLong(std::vector<const char *> &fieldNames, const char * name);
    void testNonnegDbl(std::vector<const char *> &fieldNames, const char * name);
    void testNonemptyDblVect(std::vector<const char *> &fieldNames, const char * name);
    void setNonnegLong(mwArray &cfgStruct, const char * name);
    void setNonnegDbl(mwArray &cfgStruct, const char * name);
    void setNonemptyDblVect(mwArray &cfgStruct, const char * name);

    static const char ACTUATOR_NAME[];
    static const char CONTROLLER_NAME[];
    static const char SENSOR_NAME[];

  private:
    void connect(const NcsContextComponentIndex dst);
    void recordStatistics(const mwArray &statistics, const std::string& statName);

    mwArray createNcsConfigStruct();
    mwArray createNcsPkt(RawPacket* const msg);
    simtime_t getNcsPktTimestamp(const mwArray& mw_pkt);
    bool ncsPktIsAck(const mwArray& mw_pkt);
    RawPacket* parseNcsPkt(const mwArray& mw_pkt);
    void sendNcsPkt(const mwArray& mw_pkt);
    void sendNcsPktList(const mwArray& mw_ncsPktList);
    NcsContextComponentIndex getIndexForAddr(const L3Address &addr);

    typedef RawPacket * RawPacketPtr_t;

    static const std::string ACTUATOR_GATE;
    static const std::string CONTROLLER_GATE;
    static const std::string SENSOR_GATE;
    static const std::string * GATE_NAMES[];

    static int ncsIdCounter;

    /**
     * Unique identifier for a NCS instance.
     */
    int ncsId;

    /**
     * Stores the delay before starting to poll the NCS control loop.
     * Allows network components to initialize and establish state/connections
     * before the NCS starts to communicate.
     */
    simtime_t startupDelay;

    /**
    * Stores the path of the file containing the NCS configuration.
    */
    std::string configFile;

    /**
    * Stores the interval (in pico-seconds) between two consecutive calls to ncs_doLoopStep (API-function).
    * That is, the simulation engine triggers this function periodically.
    */
    SimTime tickerInterval;

    /**
     * Stores the ComponentMap key of the corresponding NetworkedControlSystem instance in Matlab.
     */
    mwArray ncsHandle;

    /**
     * L3 Addresses for the different NCS components.
     */
    L3Address cpsAddr[NCTXCI_COUNT];

    // statistical data
    simsignal_t qocSignal;
    simsignal_t scObservedDelaySignal;
    simsignal_t caObservedDelaySignal;
    simsignal_t acObservedDelaySignal;

    simsignal_t scActualDelaySignal;
    simsignal_t caActualDelaySignal;
    simsignal_t acActualDelaySignal;
};

#endif
