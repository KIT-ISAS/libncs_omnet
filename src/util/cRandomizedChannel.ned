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

package libncs_omnet.util;

import ned.IdealChannel;

//
// A channel whose characteristics are modeled by a probability density function,
// both in terms of delay as well as losses.
// The channel has no data rate restrictions, transmission durations are always zero. 
//
channel RandomizedChannel extends IdealChannel
{
    parameters:
        @class(omnetpp::cRandomizedChannel);
    
        bool disabled = default(false);
        // probability density function, formatted as 
        // "value_1:probability_1 value_2:probability_2" (mind the space char).
        // negative values represent packet drops.
        string pdf; 
        
        @signal[messageSent](type=omnetpp::cMessage);
        @signal[messageDiscarded](type=omnetpp::cMessage);
        @statistic[messages](source="constant1(messageSent)";record=count?;interpolationmode=none);
        @statistic[messagesDiscarded](source="constant1(messageDiscarded)";record=count?;interpolationmode=none);
}
