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

#include "HistogramCollector.h"

#include <algorithm>

HistogramCollector::HistogramCollector() {
}

HistogramCollector::~HistogramCollector() {
}


void HistogramCollector::sent(const uint64_t id, const simtime_t timestamp, const bool lost) {
    samples.push_back({id, timestamp, SIMTIME_ZERO, lost});
    pktSent++;
}

simtime_t HistogramCollector::received(const uint64_t id, const simtime_t timestamp) {
    for (auto it = samples.begin(); it != samples.end(); it++) {
        if (it->pktId == id) {
            it->received = timestamp;
            pktRcvd++;

            return it->received - it->sent;
        }
    }

    EV_INFO << "Received NCS packet with id " << id << " outside of histogram collection window. "
            << "You might want to adapt the maxAge pruning parameter." << endl;

    return SIMTIME_MAX;
}

void HistogramCollector::lost(const uint64_t id) {
    for (auto it = samples.begin(); it != samples.end(); it++) {
        if (it->pktId == id) {
            it->lost = true;

            return;
        }
    }
}

void HistogramCollector::prune(const simtime_t now, const simtime_t maxAge,
        const uint minSamples, const uint maxSamples) {
    for (uint count = samples.size(); count > minSamples; count--) {
        // remove all samples until maxSamples is reached
        // then remove all additional outdated samples
        if ((count > maxSamples) || (now - samples.begin()->sent > maxAge)) {
            samples.pop_front();
        } else {
            break; // all other samples will have a newer timestamp
        }
    }
}

std::vector<double> HistogramCollector::compute(const simtime_t now,
        const simtime_t period, const uint bins, const bool presumeLoss) {
    std::vector<double> result;

    ASSERT(bins > 0);

    // no data collected yet, assume equal distribution
    if (samples.size() == 0) {
        result.resize(bins, 1.0 / bins);

        return result;
    }

    result.resize(bins, 0.0);

    // might be implemented incremental, but the overhead should be rather low
    for (auto it = samples.begin(); it != samples.end(); it++) {
        const simtime_t delay =
                (it->received >= it->sent ? it->received : now) - it->sent;
        const uint64_t bin = std::min((uint64_t) std::ceil(delay / period), (uint64_t) bins - 1);

        if (it->received >= it->sent) {
            // delay is known, write data into bin
            result[bin] += 1;
        } else if (!it->lost && !presumeLoss) {
            // delay is unknown, might be somewhere in bin..bins-1
            // assume equal distribution
            for (uint i = bin; i < bins; i++) {
                result[i] += 1.0 / (bins - bin);
            }
        } else {
            // packet is lost, or we do know nothing and presume loss
            result[bins - 1] += 1.0;
        }
    }

    for (uint i = 0; i < bins; i++) {
        result[i] = result[i] / samples.size();
    }

    return result;
}

uint HistogramCollector::sampleCount() const {
    return samples.size();
}

void HistogramCollector::resetStats() {
    pktSent = 0;
    pktRcvd = 0;

    for (auto it = samples.begin(); it != samples.end(); it++) {
        if (it->received == SIMTIME_ZERO) {
            pktRcvd--; // reduce by amount of in-flight pkts
        }
    }
}

long HistogramCollector::pktsSent() const {
    return pktSent;
}

long HistogramCollector::pktsArrived() const {
    return pktRcvd;
}

long HistogramCollector::pktsLost() const {
    long pktInFlight = 0;

    for (auto it = samples.begin(); it != samples.end(); it++) {
        if (!it->lost && it->received == SIMTIME_ZERO) {
            pktInFlight++;
        }
    }

    return pktSent - pktRcvd - pktInFlight;
}

