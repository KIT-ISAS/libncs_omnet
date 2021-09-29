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

#ifndef __LIBNCS_OMNET_COCPNMATLABNCSIMPL_H_
#define __LIBNCS_OMNET_COCPNMATLABNCSIMPL_H_

#include <omnetpp.h>

#include "CoCpnNcsContext.h"
#include "MatlabNcsImpl.h"

using namespace omnetpp;


class CoCpnMatlabNcsImpl : public AbstractCoCpnNcsImpl, public MatlabNcsImpl {

  public:

    virtual void initializeNcs(NcsContext * const context) override;

    virtual void doControlStep(const simtime_t& ncsTime, NcsContext::NcsControlStepResult * const result) override;

    virtual void setTargetQoC(const double qoc) override;
    virtual const ICoCCTranslator::RateFunction& getRateFunction() override;

    // legacy functions
    virtual ICoCCTranslator::CoCCLinearization getLinearizationForRate(const double actualQoC, const double targetQoC);
    virtual double getPktRateForQoc(const double actualQoC, const double targetQoC);
    virtual double getQoCForPktRate(const double actualQoC, const double pktRate);

  protected:

    void applyTargetQoC();

    friend class RateFunction;
    class RateFunction : public ICoCCTranslator::RateFunction {
    public:
        RateFunction(CoCpnMatlabNcsImpl * const parent);
        virtual ~RateFunction() {};

    protected:
        virtual double eval(const double targetQoC) const override;

        CoCpnMatlabNcsImpl * const parent;
    };

    virtual std::vector<const char *> getConfigFieldNames() override;
    virtual void setConfigValues(mwArray &cfgStruct) override;

  protected:

    RateFunction rateFunction = RateFunction(this);

    simtime_t lastControlStep;
    double targetQoC = -1;
    double targetQoCChanged = false;
    double rateDeviation = 1;

    simsignal_t s_rateDeviation;

};

#endif
