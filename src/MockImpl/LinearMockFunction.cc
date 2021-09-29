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

#include "LinearMockFunction.h"

Define_Module(LinearMockFunction);



void LinearMockFunction::initialize() {
    slope = par("slope").doubleValue();
    twist = par("twist").doubleValue();
    offset = par("offset").doubleValue();
}

void LinearMockFunction::handleMessage(cMessage * const msg) {
    error("LinearMockFunction received unexpected message");
}

double LinearMockFunction::getPktRateForQoc(const double actualQoC, const double targetQoC) const {
    const double x = actualQoC;
    const double y = targetQoC;

    const double m = slope;
    const double n = twist;
    const double b = offset;

    const double fa = m*n;
    const double fb = m*n*x - m;
    const double fc = b;

    const double z = fa*y*y - fb*y + fc;

    return std::max(0.0, z);
}
