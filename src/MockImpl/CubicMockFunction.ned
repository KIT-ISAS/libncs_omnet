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
// Mock model of an NCS with qubic characteristics
//
simple CubicMockFunction {
    parameters:
        
        // parameters of the qubic function ax³+bx²+cx+d, modeling the steady-state behavior
        double a = default(0.05);
        double b = default(0.0);
        double c = default(0.05);
        double d = default(0.5);
        //double c = default(0.2);
        //double d = default(0.2);
        
        // parameters of the sigmoid function (1 - x²/u) * s * tx/(1+tx²) + 1 
        // causing an increased or decreased rate for cases where actual <> target QoC
        double s = default(0.5);
        double t = default(2.0);
        double u = default(40.0);
        
}
