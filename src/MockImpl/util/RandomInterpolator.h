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

#ifndef MOCKIMPL_UTIL_RANDOMINTERPOLATOR_H_
#define MOCKIMPL_UTIL_RANDOMINTERPOLATOR_H_

#include "Interpolator.h"

using namespace omnetpp;


class RandomInterpolator: public Interpolator {
  public:

    RandomInterpolator() { };
    RandomInterpolator(const unsigned long period, cRNG * const rng, const double mean, const double spread);
    virtual ~RandomInterpolator() { };

    double getMean() const;
    double getSpread() const;

    void update();

    static RandomInterpolator createRandomInterpolator(const unsigned long period, cRNG * const rng, const double mean, const double spread);

  private:

    cRNG * rng;
    double mean;
    double spread;

  private:

    static double generate(cRNG * const rng, const double mean, const double spread);
};

#endif /* MOCKIMPL_UTIL_RANDOMINTERPOLATOR_H_ */
