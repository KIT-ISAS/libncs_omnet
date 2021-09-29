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

#include "StaticLQRMockFunction.h"

Define_Module(StaticLQRMockFunction);



void StaticLQRMockFunction::initialize() {
    normalize = par("normalize").boolValue();

    model = static_cast<Model>(par("model").intValue());

    if (model >= InvalidModel) {
        error("unknown model %d", model);
    }

    a = par("a").doubleValue();
    b = par("b").doubleValue();
    c = par("c").doubleValue();

    qoc = &par("qoc");
    rateFunction = &par("rateFunction");

    if (normalize) {
        normalize = false; // lazy hack to re-use code in getPktRate

        multiplier = 1 / getPktRateForQoc(0, 1);

        normalize = true;
    }
}

void StaticLQRMockFunction::handleMessage(cMessage * const msg) {
    error("StaticLQRMockFunction received unexpected message");
}

double StaticLQRMockFunction::getPktRateForQoc(const double actualQoC, const double targetQoC) const {
    double result;

    switch (model) {
    case Function:
        qoc->setDoubleValue(targetQoC);
        result = rateFunction->doubleValue();
        break;
    case LQRFit1:
        result = (a * targetQoC + b) / (targetQoC + c);
        break;
    default:
        error("model %d not implemented", model);
    }

    if (normalize) {
        result *= multiplier;
    }

    return result;
}
