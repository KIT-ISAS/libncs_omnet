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

import libncs_omnet.util.GenericTrafGen;

//
// A generic traffic generator module, which is capable of generating either
// simple packets or RawPackets. Derived from inet/applications/generic/IPvXTrafGen.
//
// GenericTrafGen can be re-triggered to send an arbitrary number of packet-sequences.
// To schedule a new packet sequence, a new startTime value must be 
// provided before stopTime has been reached. Both values will always be fetched
// in an alternating manner, at startTime, stopTime is fetched, and vice versa.
//
simple CameraTrafGen extends GenericTrafGen
{
    parameters:
        @display("i=block/source");

        int packetOverhead @unit("B") = default(0);
        double bpsMin @unit("bps") = default(2500bps);
        double bpsMax @unit("bps") = default(3500bps);
        
        sendInterval = 
        ( 
            (
                exp(
                    -fmod(dropUnit(simTime() - cycleStart) * (randVal+0.5) * 10, 5)
                )
                + 3
            ) 
            / 4
        ) 
        * (dropUnit(packetOverhead + packetLength) * 8)
        / (
            (
                ((randVal * cycle * 524287) % 11) / 10 
                * dropUnit(bpsMax - bpsMin) 
                + dropUnit(bpsMin)
            ) 
            * 1000
        ) 
        * 1s;
}
