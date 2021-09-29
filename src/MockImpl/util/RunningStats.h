/*
 * RunningStats.h
 *
 *  Created on: Feb 25, 2020
 *      Author: markus
 */

#ifndef MOCKIMPL_UTIL_RUNNINGSTATS_H_
#define MOCKIMPL_UTIL_RUNNINGSTATS_H_

// source: https://www.johndcook.com/blog/skewness_kurtosis/
class RunningStats {

public:
    RunningStats();
    void clear();
    void push(double x);
    unsigned long long numDataValues() const;
    double mean() const;
    double variance() const;
    double standardDeviation() const;
    double skewness() const;
    double kurtosis() const;

    friend RunningStats operator+(const RunningStats a, const RunningStats b);
    RunningStats& operator+=(const RunningStats &rhs);

private:
    unsigned long long n;
    double M1, M2, M3, M4;
};

#endif /* MOCKIMPL_UTIL_RUNNINGSTATS_H_ */
