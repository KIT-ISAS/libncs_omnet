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

#include "Interpolator.h"


Interpolator::Interpolator(const unsigned long period) : period(period), last(0), next(0) {
}

Interpolator::Interpolator(const unsigned long period, const double last, const double next) : period(period), last(last), next(next) {
}

double Interpolator::at(const unsigned long time) const {
    const unsigned long periodRelative = time % period;
    const double asFactor = ((double) periodRelative) / period;

    return next * weightFunction(asFactor) + last * weightFunction(1 - asFactor);
}

double Interpolator::weightFunction(const double x) const {
    const double shiftedX = 2 * x - 1;

    return shiftedX / (1 + shiftedX * shiftedX) + 0.5;
}

bool Interpolator::needUpdate(const unsigned long time) const {
    return time % period == 0;
}

void Interpolator::update(const double next) {
    this->last = this->next;
    this->next = next;
}

double Interpolator::getLast() const {
    return last;
}

double Interpolator::getNext() const {
    return next;
}

Interpolator Interpolator::createInterpolator(const unsigned long period, const double last, const double next) {
    return Interpolator(period, last, next);
}
