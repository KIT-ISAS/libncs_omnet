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

#include <cRandomizedChannel.h>
#include <omnetpp/cstringtokenizer.h>
#include <omnetpp/cexception.h>

#define EPSILON (1e-12)

namespace omnetpp {

Register_Class(cRandomizedChannel);

cRandomizedChannel::cRandomizedChannel(const char *name) : cIdealChannel(name) {
    disabled = false;
    pdf = std::string("").c_str();
    pdfIntervals = std::vector<PdfInterval>();
    cdfIntervals = std::vector<PdfInterval>();
}

cRandomizedChannel::~cRandomizedChannel() { }

void cRandomizedChannel::initialize() {
    messageSentSignal = registerSignal("messageSent");
    messageDiscardedSignal = registerSignal("messageDiscarded");

    rereadPars();
}

void cRandomizedChannel::handleParameterChange(const char * parname) {
    rereadPars();
}

void cRandomizedChannel::processMessage(cMessage *msg, simtime_t t, result_t& result) {
    if (disabled) {
        cTimestampedValue tmp(t, msg);

        emit(messageDiscardedSignal, &tmp);
        result.discard = true;
    } else {
        const double rand = uniform(0, 1);
        simtime_t delay = 0;

        // map uniform random number to delay using the computed distribution
        for (auto it = cdfIntervals.begin(); it != cdfIntervals.end(); it++) {
            if (rand < it->probability) {
                delay = it->value;

                break;
            }
        }

        result.delay = delay >= 0 ? delay : 0;
        result.duration = 0;
        result.discard = delay < 0;

        if (result.discard) {
            cTimestampedValue tmp(t, msg);

            emit(messageDiscardedSignal, &tmp);
        } else if (mayHaveListeners(messageSentSignal)) {
            MessageSentSignalValue tmp(t, msg, &result);

            emit(messageSentSignal, &tmp);
        }
    }
}

simtime_t cRandomizedChannel::calculateDuration(cMessage *msg) const {
    simtime_t simtimeOne = SimTime::ZERO;

    simtimeOne.setRaw(1);

    return simtimeOne;
}

simtime_t cRandomizedChannel::getTransmissionFinishTime() const {
    return simTime();
}

bool cRandomizedChannel::isBusy() const {
    return false;
}

void cRandomizedChannel::forceTransmissionFinishTime(simtime_t t) {
}

void cRandomizedChannel::rereadPars() {
    disabled = par("disabled");
    pdf = par("pdf").stringValue();

    pdfIntervals.clear();
    cdfIntervals.clear();

    cStringTokenizer pdfTokens(pdf);

    double sum = 0;

    for (const char * token = pdfTokens.nextToken(); token != nullptr;
            token = pdfTokens.nextToken()) {
        cStringTokenizer splitter(token, ":");
        std::vector<double> splits = splitter.asDoubleVector();

        if (splits.size() != 2) {
            throw cRuntimeError(this, "bad token for PDF: %s", token);
        }

        double &start = splits[0];
        double &prob = splits[1];

        if (prob < 0) {
            throw cRuntimeError(this, "probability must be positive or zero: %d", prob);
        }

        PdfInterval cur = { SimTime(start), prob };

        pdfIntervals.push_back(cur);

        sum += prob;
    }

    // prevent div by zero
    if (sum < EPSILON) {
        throw cRuntimeError(this, "token probabilities must not be all zero");
    }
    // warn if normalization is required
    if (fabs(sum - 1.0) > EPSILON) {
        EV << "WARNING: Sum of PDF elements != 1.0. They will be considered as weights and normalized accordingly";
    }
    // these two also enforced the existence of at least one token

    double cdfSum = 0;

    for (auto it = pdfIntervals.begin(); it != pdfIntervals.end(); it++) {
        // normalize and accumulate
        it->probability /= sum;
        cdfSum += it->probability;

        cdfIntervals.push_back({ it->value, cdfSum });
    }

    // deal with rounding errors
    cdfIntervals.rbegin()->probability = 1.0;
}

}
