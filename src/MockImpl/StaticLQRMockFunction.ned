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

package libncs_omnet.MockImpl;

//
// Mock model of an NCS with modified exponential characteristics
//
simple StaticLQRMockFunction {
    parameters:
        @class(StaticLQRMockFunction);
        
        // normalize rate(0)..rate(1) to [0..1]
        bool normalize = default(true);
        
        // model selection
        // 0: parametric, slow but universal
        // 1: (a * qoc + b) / (qoc + c)
        int model = default(1); 
        
        // long pendulum defaults for model 1
        double a = default(10.71);
        double b = default(-20.83);
        double c = default(-1.044);
        
        // parametric rate function (model option 0)
        volatile double rateFunction = default(qoc);
        // short pendulum: rateFunction = (9.503*qoc -20.94) / (qoc-1.049);
        // long pendulum: rateFunction = (10.71*qoc -20.83) / (qoc-1.044);
        
        
        // input, will be overridden at runtime for computations
        double qoc = default(0);
}
