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

#ifndef __LIBNCS_OMNET_COCPNNCSCONTEXT_H_
#define __LIBNCS_OMNET_COCPNNCSCONTEXT_H_

#include <omnetpp.h>

#include "NcsContext.h"
#include "CoCC/CoCCTranslator.h"

using namespace omnetpp;
using namespace inet;

// forward declaration
class AbstractCoCpnNcsImpl;

class CoCpnNcsContext : public NcsContext, public ICoCCTranslator {
  public:
    virtual ~CoCpnNcsContext() { };

  protected:
    virtual void initialize(const int stage) override;
    virtual void finish() override;

    virtual void postNetworkInit() override;
    virtual void postConnect(const NcsContextComponentIndex to) override;

  public:

    struct CoCpnNcsControlStepResult : public NcsContext::NcsControlStepResult {
        double reportedQoC;
    };

    struct CoCpnNcsParameters : public NcsContext::NcsParameters {
        bool ignoreDeadband = false;
        bool useSampledDelayProbs;
        cPar* samplingInterval;
        cPar* scDelayProbs;
        cPar* caDelayProbs;
        cPar* controllerEventBased;
        cPar* controllerEventTrigger;
        cPar* controllerDeadband;
        cPar* sensorEventBased;
        cPar* sensorMeasDelta;
        cPar* translatorFile;
    };

    typedef NcsContext::NcsSignals CoCpnNcsSignals;

    virtual CoCpnNcsParameters* getParameters() override;
    virtual CoCpnNcsSignals* getSignals() override;

  protected:

    virtual CoCpnNcsParameters* createParameters(NcsContext::NcsParameters * const parameters = nullptr) override;
    virtual CoCpnNcsSignals* createSignals(NcsContext::NcsSignals * const signals = nullptr) override;
    virtual CoCpnNcsControlStepResult* createControlStepResult() override;

    virtual AbstractNcsImpl* createNcsImpl(const std::string name) override;
    virtual void processControlStepResult(const simtime_t& ncsTime, const NcsControlStepResult * const result) override;

  private:
    AbstractCoCpnNcsImpl* ncs();

  public:
    virtual simtime_t getControlPeriod();

    virtual void setControlObserver(ICoCCTranslator::IControlObserver * const observer, void * const context = nullptr) override;

    virtual double getActualQM() override;
    virtual double getTargetQM() override;
    virtual void setTargetQM(const double targetQoC) override;

    virtual long getPayloadSize() override;
    virtual long getPerPacketOverhead() override;
    virtual void setPerPacketOverhead(const long packetOverhead) override;
    virtual long getNetworkOverhead() override;
    virtual void setNetworkOverhead(const long networkOverhead) override;

    virtual double getMaxRate() override;

    virtual ICoCCTranslator::CoCCLinearization getLinearizationForRate(const double actualQM, const double targetQM) override;
    virtual double getRateForQM(const double actualQM, const double targetQM) override;
    virtual double getQMForRate(const double actualQM, const double rate) override;
    virtual double getAvgFrequencyForQM(const double actualQM, const double targetQM) override;

  protected:
    double qocToQM(const double qoc);
    double qmToQoC(const double qm);

    friend class RateAdjustment;
    class RateAdjustment : public ICoCCTranslator::RateFunction {
    public:
        RateAdjustment(CoCpnNcsContext * const parent);
        virtual ~RateAdjustment() {};

    protected:
        virtual double eval(const double targetQM) const;

        CoCpnNcsContext * const parent;
    };

  protected:

    //
    // Variables
    //

    long perPacketOverhead = 0;
    long networkOverhead = 0;
    double actualQoC = 0;
    double targetQM = 1; // assumed to be initial default
    ICoCCTranslator::IControlObserver * observer = nullptr;
    void * observerContext = nullptr;

    double avgPayloadSize = 0;
    long payloadSizeSamples = 0;
    long lastPayloadSize = 0;

    // statistical data

    simsignal_t reportedQoC;
    simsignal_t reportedQM;
    simsignal_t targetQoCSignal;
    simsignal_t targetQMSignal;
    simsignal_t sPayloadSize;

  protected:

    //
    // parameters
    //

    long payloadSize;
    bool autoPayloadSize;
    bool useSampledDelayProbs;

    double factor;
    double offset;
};

class AbstractCoCpnNcsImpl : virtual public AbstractNcsImpl {
  public:
    virtual ~AbstractCoCpnNcsImpl() { };

    virtual void setTargetQoC(const double qoc) = 0;
    virtual const ICoCCTranslator::RateFunction& getRateFunction() = 0;
};

#endif
