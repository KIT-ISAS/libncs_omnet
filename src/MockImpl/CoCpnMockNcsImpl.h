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

#ifndef __LIBNCS_OMNET_COCPNMOCKNCSIMPL_H_
#define __LIBNCS_OMNET_COCPNMOCKNCSIMPL_H_

#include "CoCpnNcsContext.h"

#include "MockImpl/util/Interpolator.h"
#include "MockImpl/util/RandomInterpolator.h"
#include "MockImpl/util/RunningStats.h"
#include "MockImpl/util/WindowStats.h"
#include "util/HistogramCollector.h"
#include "util/FunctionTools.h"

#include <omnetpp.h>


using namespace omnetpp;


class CoCpnMockNcsImpl : public AbstractCoCpnNcsImpl, public cSimpleModule {

  public:

    virtual void initialize() override;
    virtual void handleMessage(cMessage * const msg) override;

  public:

    virtual void initializeNcs(NcsContext * const context) override;
    virtual void finishNcs() override;

    virtual const simtime_t& getPlantPeriod() override;
    virtual const simtime_t& getControlPeriod() override;

    virtual void doPlantStep(const simtime_t& ncstTime, NcsContext::NcsPlantStepResult * const result) override;
    virtual void doControlStep(const simtime_t& ncsTime, NcsContext::NcsControlStepResult * const result) override;
    virtual std::vector<NcsContext::NcsPkt> handlePacket(const simtime_t& ncsTime, NcsContext::NcsPkt& ncsPkt) override;

  private:

    double computeRecentPktUtility();
    void updateLongTermUtilityPrediction();
    double computeUtilityForPkt(const simtime_t delay);
    NcsContext::NcsPkt createPkt(const size_t len);

  public:

    class MockFunction {
      public:
        virtual ~MockFunction() {};

        // regular pktRate value range is [0,1] for all methods
        virtual double getPktRateForQoc(const double actualQoC, const double targetQoC) const = 0; // raw unscaled model function
    };

  protected:

    friend class ScaledMockFunction;
    friend class PublicScaledMockFunction;

    class ScaledMockFunction : public ICoCCTranslator::RateFunction {
    public:
        ScaledMockFunction(CoCpnMockNcsImpl * const parent);
        virtual ~ScaledMockFunction() {};

    protected:
        virtual double eval(const double targetQoC) const override;
        double predictionScaling(const double factor, const double x) const;

        CoCpnMockNcsImpl * const parent;
    };

    class PublicScaledMockFunction : public ICoCCTranslator::RateFunction {
    public:
        PublicScaledMockFunction(ScaledMockFunction &f, simtime_t &interval);
        virtual ~PublicScaledMockFunction() {};

    protected:
        virtual double eval(const double targetQoC) const override;

        ScaledMockFunction &f;
        simtime_t &interval;
    };

  public:
    virtual void setTargetQoC(const double targetQoC) override;
    virtual const ICoCCTranslator::RateFunction& getRateFunction() override;

  private:
    double getQoCForPktRate(const double rate);
    double getPredictionScalingFactor();

  protected:

    // general context

    NcsContext * context;
    const NcsContext::NcsParameters * parameters;
    const NcsContext::NcsSignals * signals;
    MockFunction * function;
    ScaledMockFunction rawScaledFunction = ScaledMockFunction(this);
    PublicScaledMockFunction pubScaledFunction = PublicScaledMockFunction(rawScaledFunction, tickerInterval);

    // parameters and derived variables

    bool fillRawPackets;

    std::string mockFunction;

    simtime_t tickerInterval;

    unsigned long maxPktDelay;
    double delayUtilityFactor;
    double delayUtilityOffset;
    double delayUtilityLimit;

    unsigned long historySamplingSteps;
    unsigned long utilitySamplingSteps;

    bool useRatePrediction;
    unsigned long ratePredictionShortWindowSteps;
    unsigned long ratePredictionLongWindowSteps;
    double ratePredictionTruncationThreshold;
    unsigned long ratePredictionTruncatedLongWindowSteps;

    double rateJitter;

    unsigned long sensorPayload;
    unsigned long controllerPayload;

    RandomInterpolator qocRandomizer;
    RandomInterpolator rateRandomizer;

    std::vector<double> utilityWeights;

    // internal state

    std::deque<double> qocValues;
    RunningWindowStats<double> rateValues;
    RunningWindowStats<int,int> pktCount;
    ChainedRunningWindowStats<double> pktUtilityHistory { ChainedRunningWindowStats<double>(2) };
    HistogramCollector caHist;

    double predictedPktUtility;

    double predictedQoC;
    double actualQoC;
    double activeTargetQoC;
    double nextTargetQoC;
    bool targetQoCChanged;

    unsigned long step;
    unsigned long pktCounter;
    double rateAccumulator;
    double ratePhaseShift;
    double phaseShiftTracker;
    double lastPhaseShiftRate;

    // signals

    simsignal_t s_predictedPktUtility;
    simsignal_t s_predictedPktUtilityVariance;
    simsignal_t s_shortPredictedPktUtility;
    simsignal_t s_shortPredictedPktUtilityVariance;
    simsignal_t s_pktUtility;
    simsignal_t s_unscaledPktUtility;
    simsignal_t s_predictedQoC;
    simsignal_t s_avgQoC;
    simsignal_t s_actQoC;
    simsignal_t s_avgRate;
    simsignal_t s_actRate;
    simsignal_t s_rateAccumulator;
    simsignal_t s_ratePhaseShift;
};

#endif
