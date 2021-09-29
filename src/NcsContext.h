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

#include "util/HistogramCollector.h"

using namespace omnetpp;
using namespace inet;

enum NcsContextMessageKind {
    NCTXMK_TICKER_EVT = 2300,
    NCTXMK_STARTUP_POLL_EVT,
    NCTXMK_STARTUP_STATS_EVT
};

enum NcsContextComponentIndex {
    NCTXCI_ACTUATOR = 0,
    NCTXCI_CONTROLLER,
    NCTXCI_SENSOR,
    NCTXCI_COUNT
};

enum NcsContextControllerFailureAction {
    NCTXCFA_IGNORE = 0,
    NCTXCFA_FINISH,
    NCTXCFA_ABORT
};

// forward declaration
class AbstractNcsImpl;

class NcsContext : public cSimpleModule {
  public:
    virtual ~NcsContext();

  protected:
    virtual int numInitStages() const override;
    virtual void initialize(const int stage) override;
    virtual void finish() override;
    virtual void finishNcs();
    virtual void handleMessage(cMessage * const msg) override;

    virtual void postNetworkInit() { };
    virtual void postConnect(const NcsContextComponentIndex to) { };

  public:

    static const std::string NCS_ACTUATOR;
    static const std::string NCS_CONTROLLER;
    static const std::string NCS_SENSOR;
    static const std::string * NCS_NAMES[];

    struct NcsPkt {
        NcsContextComponentIndex src;
        NcsContextComponentIndex dst;
        uint64_t pktId;
        bool isAck;
        RawPacket* pkt;
    };

    struct NcsControlStepResult {
        virtual ~NcsControlStepResult() { };

        bool controllerStateAdmissible;
        std::vector<NcsPkt> pkts;
    };

    struct NcsPlantStepResult {
        virtual ~NcsPlantStepResult() { };

        bool plantStateAdmissible;
    };

    struct NcsParameters {
        virtual ~NcsParameters() { };

        int ncsId;
        simtime_t startupDelay;
        simtime_t simTimeLimit;
        cPar* configFile;
        cPar* controllerClassName;
        cPar* filterClassName;
        cPar* networkType;
        cPar* controlSequenceLength;
        cPar* maxMeasDelay;
        cPar* mpcHorizon;
        cPar* controlErrorWindowSize;
    };

    struct NcsSignals {
        virtual ~NcsSignals() { };

        simsignal_t controlErrorSignal;
        simsignal_t estControlErrorSignal;
        simsignal_t stageCostsSignal;

        simsignal_t scObservedDelaySignal;
        simsignal_t caObservedDelaySignal;
        simsignal_t acObservedDelaySignal;
    };

    struct NcsDelays {
        std::vector<double> sc;
        std::vector<double> ca;
    };

    virtual NcsParameters* getParameters() { return ncsParameters; };
    virtual NcsSignals* getSignals() { return ncsSignals; };
    NcsDelays computeDelays();
    void updateControlPeriod(const simtime_t newControlPeriod); // must only be called from a call context within doControlStep() or processControlStepResult()

  private:

    struct CommunicationStatus {
        bool sc;
        bool ca;
        bool ac;
    };

    void handleControllerFailure();

    bool setupNCSConnections();
    void connect(const NcsContextComponentIndex dst);

    CommunicationStatus sendNcsPacketsToNetwork(const std::vector<NcsPkt> pkts);
    void sendNcsPacketToNetwork(const NcsPkt& ncsPkt, CommunicationStatus& cs);
    void handleNcsPacketFromNetwork(RawPacket* const rawPkt);

    NcsContextComponentIndex getIndexForAddr(const L3Address &addr);

  protected:

    virtual NcsParameters* createParameters(NcsParameters * const parameters = nullptr);
    virtual NcsSignals* createSignals(NcsSignals * const signals = nullptr);
    virtual NcsControlStepResult* createControlStepResult();
    virtual NcsPlantStepResult* createPlantStepResult();

    virtual AbstractNcsImpl* createNcsImpl(const std::string name);
    virtual void doPlantStep(const simtime_t& ncsTime);
    virtual void doControlStep(const simtime_t& ncsTime);
    virtual void processPlantStepResult(const simtime_t& ncsTime, const NcsPlantStepResult * const result);
    virtual void processControlStepResult(const simtime_t& ncsTime, const NcsControlStepResult * const result);
    virtual void ncsRuntimeLimitReached() { };

  protected:

    //
    // Variables
    //

    static int ncsIdCounter;

    /**
     * Actual NCS instance
     */
    AbstractNcsImpl * ncs = nullptr;

    /**
     * Parameters for NCS
     */
    NcsParameters * ncsParameters = nullptr;

    /**
     * Signals for NCS
     */
    NcsSignals * ncsSignals = nullptr;

    /**
     * Struct used to keep the results of the last NCS control step operation
     */
    NcsControlStepResult * ncsControlStepResult = nullptr;

    /**
     * Struct used to keep the results of the last NCS plant step operation
     */
    NcsPlantStepResult * ncsPlantStepResult = nullptr;

    /**
     * Unique identifier of the NCS instance.
     */
    int ncsId;

    /**
     * The interval between two consecutive plant computation steps;
     */
    simtime_t plantPeriod;

    /**
    * The interval between two consecutive control computation steps.
    */
    simtime_t controlPeriod;

    /**
     * Time at which the next plant computation step should be done.
     */
    simtime_t nextPlantStep;

    /**
     * Time at which the next control computation step should be done.
     */
    simtime_t nextControlStep;

    /**
     * Determines whether the connections between sensor, controller and actuator are initiated.
     */
    bool networkConfigured;

    /**
     * Observed plant/controller state has been admissible so far?
     */
    bool plantStateAdmissible;
    bool controllerStateAdmissible;

    /**
     * L3 Addresses for the different NCS components.
     */
    L3Address cpsAddr[NCTXCI_COUNT];

    /**
     * Histogram collectors for sensor-controller, controller-actuator and actuator-controller paths.
     */
    HistogramCollector scHist;
    HistogramCollector caHist;
    HistogramCollector acHist;

    // statistical data
    simsignal_t scSentSignal;
    simsignal_t caSentSignal;
    simsignal_t acSentSignal;

    simsignal_t scActualDelaySignal;
    simsignal_t caActualDelaySignal;
    simsignal_t acActualDelaySignal;

    simsignal_t controlPeriodSignal;

  protected:

    //
    // parameters
    //

    /**
     * name of the NCS implementation to use
     */
    std::string ncsImplName;

    /**
     * Stores the polling interval used to determine whether the OMNeT++/INET networking
     * stack has finished initialization. A positive value will be required
     * if hosts do get their IP addresses pre-assigned during initialization.
     * MatlabNcsContext will try to initiate connections between NCS hosts at the
     * given polling interval.
     */
    simtime_t networkStartupPollInterval;

    /**
     * Stores the delay before starting to poll the NCS control loop.
     * Allows network components to initialize and establish state/connections
     * before the NCS starts to communicate.
     */
    simtime_t startupDelay;

    /**
     * Time for which the NCS should be simulated.
     * NCS simulation will be stopped at startupDelay + simulationRuntime.
     * Negative values == unlimited runtime
     */
    simtime_t simulationRuntime;

    /**
     * Determines what should be done if the controller signals an error condition,
     * e.g. in case the plant is in a not admissible state.
     * 0: ignore (default), 1: stop simulation gracefully, 2: abort simulation with exception
     */
    int actionOnControllerFailure;

    /**
     * minimum number of samples used for histogram (will not be pruned even if outdated)
     */
    int minSampleCount;
    /**
     * maximum number of samples used for histogram (will prune oldest samples even if not outdated)
     */
    int maxSampleCount;
    /**
     * sample age (in seconds) after which samples get pruned
     */
    double maxSampleAge;
    /**
     * number of sample bins, each spaced in control sampling rate intervals
     */
    int maxSampleBins;
    /**
     * report time steps in which the controller did not send a control sequence
     * as lost packet? useful for event-triggered communication to account for
     * packets which were suppressed without knowledge of the controller
     */
    bool reportUnusedStepsAsLoss;
    /**
     * point in time at which pkt statistics are reset (for evaluation purposes)
     */
    simtime_t pktStatisticsStartDelay;
};

class AbstractNcsImpl {
  public:
    virtual ~AbstractNcsImpl() { };

    virtual void initializeNcs(NcsContext * const context) = 0;
    virtual void finishNcs() = 0;

    virtual const simtime_t& getPlantPeriod() = 0;
    virtual const simtime_t& getControlPeriod() = 0;

    virtual void doPlantStep(const simtime_t& ncsTime, NcsContext::NcsPlantStepResult * const result) = 0;
    virtual void doControlStep(const simtime_t& ncsTime, NcsContext::NcsControlStepResult * const result) = 0;
    virtual std::vector<NcsContext::NcsPkt> handlePacket(const simtime_t& ncsTime, NcsContext::NcsPkt& ncsPkt) = 0;
};

#endif
