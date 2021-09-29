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

#ifndef __LIBNCS_OMNET_STATICLQRMOCKFUNCTION_H_
#define __LIBNCS_OMNET_STATICLQRMOCKFUNCTION_H_

#include "CoCpnMockNcsImpl.h"

#include <omnetpp.h>

using namespace omnetpp;


class StaticLQRMockFunction : public CoCpnMockNcsImpl::MockFunction, public cSimpleModule {
  public:

    virtual void initialize() override;
    virtual void handleMessage(cMessage * const msg) override;

    virtual double getPktRateForQoc(const double actualQoC, const double targetQoC) const override;

  protected:

    enum Model {
        Function = 0,
        LQRFit1, // (a * qoc + b) / (qoc + c)
        InvalidModel // no real model but end of valid range
    };

  private:

    bool normalize;

    Model model;

    double a, b, c;

    cPar * qoc;
    cPar * rateFunction;

    double multiplier;

};

#endif
