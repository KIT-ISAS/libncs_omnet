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

#include <util/FunctionTools.h>

FunctionTools::Clamp FunctionTools::Clamp::UNBOUNDED = FunctionTools::Clamp(DBL_MIN, DBL_MAX);

FunctionTools::Clamp::Clamp(const double min, const double max)
        : min(min), max(max) {
    ASSERT(min <= max);
}

double FunctionTools::Clamp::eval(const double x) const {
    return CLAMP(x, min, max);
}

bool FunctionTools::operator==(const FunctionTools::Clamp &lhs, const FunctionTools::Clamp &rhs) {
    return lhs.min == rhs.min && lhs.max == rhs.max;
}

bool FunctionTools::operator!=(const FunctionTools::Clamp &lhs, const FunctionTools::Clamp &rhs) {
    return !(lhs == rhs);
}


FunctionTools::UnaryFunction::UnaryFunction(const Clamp limit)
        : limit(limit) {
}


FunctionTools::Derivative::Derivative(const UnaryFunction &f, const double h)
        : FunctionTools::UnaryFunction(f.limit), f(f), h(h) {
}

double FunctionTools::Derivative::eval(const double x) const {
    const double x_ = limit(x);

    const double deltaPh = limit(x_ + h) - x_;
    const double deltaMh = limit(x_ - h) - x_;
    const double fPh = f(x_ + deltaPh);
    const double fMh = f(x_ + deltaMh);

    const double result = (fPh - fMh) / (deltaPh - deltaMh);

    return result;
}


FunctionTools::BisectSolve::BisectSolve(const UnaryFunction &f, const FunctionTools::Clamp limit, const unsigned int iterations, const double eps)
        : f(f), limit(limit), iterations(iterations), eps(eps) {
}

double FunctionTools::BisectSolve::solve(const double y, const double x_start) const {
    double min = limit.min;
    double max = limit.max;
    double result = limit(x_start);

    for (unsigned int i = 0; i < iterations; i++) {
        const double deltaY = f(result) - y;

        if (std::abs(deltaY) < eps) {
            break; // solution is sufficiently precise, abort
        }

        if (deltaY > 0) {
            max = result;
        } else {
            min = result;
        }

        result = min + (max - min)/2;
    }

    return result;
}

FunctionTools::NewtonSolve::NewtonSolve(const UnaryFunction &f, const FunctionTools::Clamp limit, const unsigned int iterations, const double eps)
        : f(f),
          derive(Derivative(f)),
          bisect(BisectSolve(f, limit, 8, 1E-3)),
          limit(limit),
          iterations(iterations),
          eps(eps) {
}

FunctionTools::NewtonSolve::NewtonSolve(const Derivative derive, const BisectSolve bisect, const unsigned int iterations, const double eps)
        : f(derive.f),
          derive(derive),
          bisect(bisect),
          limit(bisect.limit),
          iterations(iterations),
          eps(eps) {
    ASSERT(&derive.f == &bisect.f);
}

double FunctionTools::NewtonSolve::solve(const double y, const double x_start) const {
    EV_STATICCONTEXT;

    double x = limit(x_start);
    unsigned int i = 0;

    // catch cases where the requested rate is outside the possible range
    if (y <= f(limit.min)) {
        return 0;
    }
    if (y >= f(limit.max)) {
        return 1;
    }

    // approximate using Newton's method
    do {
        const double f1 = derive(x);

        // detect if m is too small and shift y to move away from plateau
        if (f1 < eps) {
            EV_DEBUG << "NewtonSolve(" << x_start << ", " << y << ") hit plateau at x=" << x << std::endl;

            x = bisect.solve(y, x);

            EV_DEBUG << "continuing after bisection at x=" << x << std::endl;

            continue;
        }

        x = x - (f(x) - y) / f1;
        x = limit(x); // keep argument within range

        if (std::abs(f(x) - y) < eps) {
            EV_DEBUG << "NewtonSolve(" << x_start << ", " << y << ") converged within " << i << " iterations." << std::endl;

            break;
        }
    } while (i++ < iterations);

    if (i > iterations) {
        EV_WARN << "NewtonSolve(" << x_start << ", " << y << ") did not converge within " << iterations << " iterations. computed x=" << x << std::endl;
    }

    return limit(x);
}
