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

#include "RandomInterpolator.h"

RandomInterpolator::RandomInterpolator(const unsigned long period, cRNG * const rng, const double mean, const double spread) :
        Interpolator(period, generate(rng, mean, spread), generate(rng, mean, spread)),
        rng(rng),
        mean(mean),
        spread(spread) {
}

double RandomInterpolator::getMean() const {
    return mean;
}

double RandomInterpolator::getSpread() const {
    return spread;
}

void RandomInterpolator::update() {
    const double next = generate(rng, mean, spread);

    Interpolator::update(next);
}

RandomInterpolator RandomInterpolator::createRandomInterpolator(const unsigned long period, cRNG * const rng, const double mean, const double spread) {
    return RandomInterpolator(period, rng, mean, spread);
}

double RandomInterpolator::generate(cRNG * const rng, const double mean, const double spread) {
    ASSERT(rng);

    return uniform(rng, mean - spread, mean + spread);
}
