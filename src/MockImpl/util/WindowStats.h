/*
 * WindowStats.h
 *
 *  Created on: Feb 25, 2020
 *      Author: markus
 */

#ifndef MOCKIMPL_UTIL_WINDOWSTATS_H_
#define MOCKIMPL_UTIL_WINDOWSTATS_H_

#include <assert.h>
#include <deque>

template<typename Tval, typename Tagg = double>
class WindowStats {

public:

    struct Sample {
        Tval value;
        bool valid;
    };

    WindowStats() { }

    WindowStats(const unsigned long windowSize) {
        resize(windowSize);
    }

    WindowStats(const unsigned long windowSize, const Tval value) {
        resize(windowSize, value);
    }

    virtual ~WindowStats() { };


    virtual void reset() {
        buf.clear();
    }

    virtual void reset(const Tval value) {
        buf.assign(buf.size(), value);
    }

    virtual void truncate(const unsigned long size) {
        if (buf.size() > size) {
            buf.resize(size);
        }
    }

    virtual void resize(const unsigned long size) {
        assert(size > 1);

        this->sizeLimit = size;

        truncate(size);
    }

    virtual void resize(const unsigned long size, const Tval value) {
        assert(size > 1);

        this->sizeLimit = size;
        buf.resize(size, value);
    }

    unsigned long maxSize() const {
        return this->sizeLimit;
    }

    unsigned long windowSize() const {
        return buf.size();
    }

    virtual Sample push(const Tval value) {
        assert(sizeLimit > 1);

        Sample result;

        if (buf.size() == sizeLimit) {
            result.value = buf.back();
            result.valid = true;

            buf.pop_back();
        } else {
            result.valid = false;
        }

        buf.push_front(value);

        return result;
    }


    virtual Tval sum() const {
        Tval sum = 0;

        for (auto val : buf) {
            sum += val;
        }

        return sum;
    }

    virtual Tagg mean() const {
        if (buf.size() == 0) {
            return 0;
        }

        Tagg meanValue = sum();
        meanValue /= buf.size();

        return meanValue;
    }

    virtual Tagg variance(const Tagg epsilon = 1E-7) const {
        if (buf.size() == 0) {
            return 0;
        }

        const Tagg m = mean();
        Tagg sum = 0;

        for (auto val : buf) {
            const Tagg diff = val - m;

            sum += diff * diff;
        }

        if (sum > epsilon) {
            return sum / (buf.size() - 1);
        } else {
            return -1;
        }
    }

protected:

    unsigned long sizeLimit = 2;
    std::deque<Tval> buf = { };

};

template<typename Tval, typename Tagg = double>
class RunningWindowStats : public WindowStats<Tval, Tagg> {

public:

    RunningWindowStats() { }

    RunningWindowStats(const unsigned long windowSize) {
        resize(windowSize);
    }

    RunningWindowStats(const unsigned long windowSize, const Tval value) {
        resize(windowSize, value);
    }


    virtual void recomputeStats() {
        pushCount = 0;

        simpleSum = WindowStats<Tval, Tagg>::sum();
        squaredSum = sumSquared();
    }

    virtual void reset() override {
        WindowStats<Tval, Tagg>::reset();

        pushCount = 0;

        simpleSum = 0;
        squaredSum = 0;
    }

    virtual void reset(const Tval value) override {
        WindowStats<Tval, Tagg>::reset(value);

        pushCount = 0;

        simpleSum = this->buf.size() * value;
        squaredSum = this->buf.size() * value * value;
    }

    virtual void truncate(const unsigned long size) override {
        const unsigned long oldSize = this->buf.size();

        WindowStats<Tval, Tagg>::truncate(size);

        if (oldSize != this->buf.size()) {
            recomputeStats();
        }
    }

    virtual void resize(const unsigned long size) override {
        WindowStats<Tval, Tagg>::resize(size); // calls truncate internally
    }

    virtual void resize(const unsigned long size, const Tval value) override {
        WindowStats<Tval, Tagg>::resize(size, value);

        recomputeStats();
    }

    virtual typename WindowStats<Tval, Tagg>::Sample push(const Tval value) {
        const auto old = WindowStats<Tval, Tagg>::push(value);

        if (old.valid) {
            simpleSum -= old.value;
            squaredSum -= old.value*old.value;
        }

        simpleSum += value;
        squaredSum += value*value;

        // trigger recomputation to prevent increasing errors
        if (pushCount++ > 10000) {
            recomputeStats();
        }

        return old;
    }

    Tval sumSquared() const {
        Tval result = 0;

        for (auto val : this->buf) {
            result += val*val;
        }

        return result;
    }

    virtual Tval sum() const {
        return simpleSum;
    }

    virtual Tagg mean() const {
        Tagg meanValue = simpleSum;
        meanValue /= this->buf.size();

        return meanValue;
    }

    virtual Tagg variance(const Tagg epsilon = 1E-7) const {
        if (this->buf.size() == 0) {
            return 0;
        }

        Tagg result = squaredSum - (simpleSum * simpleSum) / this->buf.size();

        if (result > epsilon) {
            return result / (this->buf.size() - 1);
        } else {
            return -1;
        }
    }

protected:

    unsigned long pushCount = 0;
    Tval simpleSum = 0;
    Tval squaredSum = 0;

};

template<typename Tval, typename Tagg = double>
class ChainedRunningWindowStats {

public:

    ChainedRunningWindowStats(uint numStats) {
        stats.resize(numStats);

        reset();
    }

    void reset() {
        for (auto stat : stats) {
            stat.reset();
        }
    }

    void reset(const Tval value) {
        for (auto stat : stats) {
            stat.reset(value);
        }
    }

    void truncate(const unsigned long size) {
        unsigned long remaining = size;

        for (auto it = stats.begin(); it != stats.end(); it++) {
            it->truncate(remaining);

            const unsigned long window = it->windowSize();
            const unsigned long change = std::min(remaining, window);

            remaining -= change;
        }
    }

    void push(const Tval value) {
        typename WindowStats<Tval, Tagg>::Sample carry;

        carry.value = value;
        carry.valid = true;

        for (auto it = stats.begin(); carry.valid && it != stats.end(); it++) {
            carry = it->push(carry.value);
        }
    }

public:

    std::vector<RunningWindowStats<Tval, Tagg>> stats;

};

template<typename Tval, typename Tagg = double>
class OverlappingRunningWindowStats {

public:

    OverlappingRunningWindowStats(uint numStats) {
        stats.resize(numStats);

        reset();
    }

    void reset() {
        for (auto stat : stats) {
            stat.reset();
        }
    }

    void reset(const Tval value) {
        for (auto stat : stats) {
            stat.reset(value);
        }
    }

    void truncate(const unsigned long size) {
        for (auto it = stats.begin(); it != stats.end(); it++) {
            it->truncate(size);
        }
    }

    void push(const Tval value) {
        typename WindowStats<Tval, Tagg>::Sample carry;
        for (auto it = stats.begin(); it != stats.end(); it++) {
            it->push(value);
        }
    }

public:

    std::vector<RunningWindowStats<Tval, Tagg>> stats;

};

/*
template<typename Tval, typename Tagg = double>
class VarianceTriggeredWindowFilter {

public:

    VarianceTriggeredWindowFilter(const unsigned long firstWindow, const unsigned long steps, const double triggerSigma, const double varianceThreshold) {
        stats.resize(steps);

        for (unsigned long i = 0; i < steps; i++) {
            stats.stats[i].resize(firstWindow * ((unsigned long)1 << i));
        }

        reset();

        this->triggerSigma = triggerSigma;
        this->varianceThreshold = varianceThreshold;
        this->activeStage = -1;
    }

    void reset() {
        stats.reset();
    }

    void reset(const Tval value) {
        stats.reset(value);
    }

    void truncate(const unsigned long size) {
        stats.truncate(size);
    }

    Tagg filter(const Tval value) {


        lastResult;

        stats.push(value);

        return lastResult;
    }

    Tagg getLastValue() {
        return lastResult;
    }


public:

    OverlappingRunningWindowStats<Tval, Tagg> stats;
    Tagg lastResult;
    unsigned long triggerSigma;
    unsigned long varianceThreshold;
    long activeStage;

};
*/

#endif /* MOCKIMPL_UTIL_WINDOWSTATS_H_ */
