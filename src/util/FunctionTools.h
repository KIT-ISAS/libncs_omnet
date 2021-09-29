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

#ifndef UTIL_FUNCTIONTOOLS_H_
#define UTIL_FUNCTIONTOOLS_H_

#include <omnetpp.h>

#define CLAMP(clamp_x, clamp_min, clamp_max) std::min(std::max((clamp_x), (clamp_min)), (clamp_max))

namespace FunctionTools {

    class Clamp {
    public:
        Clamp(const double min, const double max);
        double eval(const double x) const;
        double operator()(const double x) const { return eval(x); };

        friend bool operator==(const Clamp &lhs, const Clamp &rhs);
        friend bool operator!=(const Clamp &lhs, const Clamp &rhs);

        const double min, max;

        static Clamp UNBOUNDED;
    };
    bool operator==(const Clamp &lhs, const Clamp &rhs);
    bool operator!=(const Clamp &lhs, const Clamp &rhs);


    class UnaryFunction {
    public:
        UnaryFunction(const Clamp limit = Clamp::UNBOUNDED);
        virtual ~UnaryFunction() { };
        double operator()(const double x) const { return eval(limit(x)); };

        const Clamp limit;

    protected:
        virtual double eval(const double x) const { return x; };
    };


    class BinaryFunction {
    public:
        virtual ~BinaryFunction() { };
        double operator()(const double x, const double y) const { return eval(x, y); };

    protected:
        virtual double eval(const double x, const double y) const = 0;
    };


    class Derivative : public UnaryFunction {
    public:
        Derivative(const UnaryFunction &f, const double h = 1E-5);

        const UnaryFunction &f;
        const double h;

    protected:
        double eval(const double x) const override;
    };


    class BisectSolve : public BinaryFunction {
    public:
        BisectSolve(const UnaryFunction &f, const Clamp limit = Clamp::UNBOUNDED, const unsigned int iterations = 128, const double eps = 1E-8);
        double solve(const double y, const double x_start = 0) const;

        const UnaryFunction &f;
        const Clamp limit;
        const unsigned int iterations;
        const double eps;

    protected:
        double eval(const double x, const double y) const override { return solve(y, x); };
    };


    class NewtonSolve : public BinaryFunction {
    public:
        NewtonSolve(const UnaryFunction &f, const Clamp limit = Clamp::UNBOUNDED, const unsigned int iterations = 32, const double eps = 1E-8);
        NewtonSolve(const Derivative derive, const BisectSolve bisect, const unsigned int iterations = 32, const double eps = 1E-8);
        double solve(const double y, const double x_start = 0) const;

        const UnaryFunction &f;
        const Derivative derive;
        const BisectSolve bisect;
        const Clamp limit;
        const unsigned int iterations;
        const double eps;

    protected:
        double eval(const double x, const double y) const override { return solve(y, x); };
    };

};

#endif /* UTIL_FUNCTIONTOOLS_H_ */
