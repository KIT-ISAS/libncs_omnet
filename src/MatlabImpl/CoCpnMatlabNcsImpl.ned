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

package libncs_omnet.MatlabImpl;

import libncs_omnet.MatlabImpl.MatlabNcsImpl;

//
// Implementation of a adaptor to a MATLAB-based NCS instance with support for
// enhanced information exchange between control and communication
//
simple CoCpnMatlabNcsImpl extends MatlabNcsImpl
{
    @class(CoCpnMatlabNcsImpl);
    
    @signal[rateDeviation](type="double");
    
    @statistic[rateDeviation](source=rateDeviation;title="Rate deviation factor";record=stats?,vector?;interpolationmode=sample-hold);
    
}
