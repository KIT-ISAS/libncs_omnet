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

#include "CubicMockFunction.h"

Define_Module(CubicMockFunction);



void CubicMockFunction::initialize() {
    a = par("a").doubleValue();
    b = par("b").doubleValue();
    c = par("c").doubleValue();
    d = par("d").doubleValue();

    s = par("s").doubleValue();
    t = par("t").doubleValue();
    u = par("u").doubleValue();
}

void CubicMockFunction::handleMessage(cMessage * const msg) {
    error("CubicMockFunction received unexpected message");
}

double CubicMockFunction::getPktRateForQoc(const double actualQoC, const double targetQoC) const {
    const double x = actualQoC;
    const double y = targetQoC;

    const double xPy = x + y;
    const double yMx = y - x;

    const double sigmoidFactor = s * t * yMx / (1 + t * yMx*yMx);
    const double sigmoidDamping = 1 - (xPy*xPy) / u;
    const double sigmoidTotal = sigmoidDamping * sigmoidFactor + 1;

    const double cubic = a * xPy*xPy*xPy + b * xPy*xPy + c * xPy + d;

    const double z = sigmoidTotal * cubic;

    return z; // unbounded!
}
