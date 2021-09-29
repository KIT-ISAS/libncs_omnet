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

#ifndef UTIL_HISTOGRAMCOLLECTOR_H_
#define UTIL_HISTOGRAMCOLLECTOR_H_

#include <omnetpp.h>
#include <deque>

using namespace omnetpp;

class HistogramCollector {

public:
    HistogramCollector();
    virtual ~HistogramCollector();

    void sent(const uint64_t id, const simtime_t timestamp, const bool lost = false);
    simtime_t received(const uint64_t id, const simtime_t timestamp);
    void lost(const uint64_t id);
    void prune(const simtime_t now, const simtime_t maxAge,
            const uint minSamples, const uint maxSamples);
    std::vector<double> compute(const simtime_t now, const simtime_t period,
            const uint bins, const bool presumeLoss = false);

    uint sampleCount() const;

    void resetStats();
    long pktsSent() const;
    long pktsArrived() const;
    long pktsLost() const;

protected:
    struct Sample {
        uint64_t pktId;
        simtime_t sent;
        simtime_t received;
        bool lost;
    };

    typedef std::deque<Sample> SampleDeque_t;
    SampleDeque_t samples;

    long pktSent = 0;
    long pktRcvd = 0;
};

#endif /* UTIL_HISTOGRAMCOLLECTOR_H_ */
