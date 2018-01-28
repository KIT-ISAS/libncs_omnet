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

//
// A generic traffic generator module, which is capable of generating either
// simple packets or RawPackets. Derived from inet/applications/generic/IPvXTrafGen.
//
simple GenericTrafGen
{
    parameters:
        @display("i=block/source");

        // generate RawPackets (if true), or plain cPackets (if false) 
        bool generateRaw = default(false);
        // time of sending the first packet
        double startTime @unit("s") = default(1s); 
        // time of finishing sending, negative values mean forever
        double stopTime @unit("s") = default(-1s);  
        // may be a random value, e.g. exponential(1)
        volatile double sendInterval @unit("s") = default(10ms); 
        // max number of packets to generate, -1 means forever
        int numPackets = default(-1); 
        // packet length in bytes
        volatile int packetLength @unit("B"); 

        @signal[sentPk](type=cPacket);
        @signal[rcvdPk](type=cPacket);
        @statistic[rcvdPk](title="packets received"; source=rcvdPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[sentPk](title="packets sent"; source=sentPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[endToEndDelay](title="end-to-end delay"; source="messageAge(rcvdPk)"; unit=s; record=histogram,vector; interpolationmode=none);

    gates:
        inout io;
}
