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

package libncs_omnet.NSC;

import inet.transportlayer.tcp_nsc.TCP_NSC;
import inet.transportlayer.contract.ITCP;

//
// Extends TCP_NSC to expose the sysctl interface. Arbitrary sysctl calls can
// be executed by setting up the according parameter.
//
simple TCP_NSC_Sysctl extends TCP_NSC
{
    parameters:
        @class(::TCP_NSC_Sysctl);
        
        // A whitespace seperated list of sysctl statements which will be 
        // executed during initialization.
        string sysctl = default("");
}
