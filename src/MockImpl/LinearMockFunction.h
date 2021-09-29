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

#ifndef __LIBNCS_OMNET_LINEARMOCKFUNCTION_H_
#define __LIBNCS_OMNET_LINEARMOCKFUNCTION_H_

#include "CoCpnMockNcsImpl.h"

#include <omnetpp.h>

using namespace omnetpp;


class LinearMockFunction : public CoCpnMockNcsImpl::MockFunction, public cSimpleModule {
  public:

    virtual void initialize() override;
    virtual void handleMessage(cMessage * const msg) override;

    virtual double getPktRateForQoc(const double actualQoC, const double targetQoC) const override;

  private:

    double slope;
    double twist;
    double offset;

};

#endif
