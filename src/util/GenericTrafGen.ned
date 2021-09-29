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

import libncs_omnet.util.ITrafGen;

//
// A generic traffic generator module, which is capable of generating either
// simple packets or RawPackets. Derived from inet/applications/generic/IPvXTrafGen.
//
// GenericTrafGen can be re-triggered to send an arbitrary number of packet-sequences.
// To schedule a new packet sequence, a new startTime value must be 
// provided before stopTime has been reached. Both values will always be fetched
// in an alternating manner, at startTime, stopTime is fetched, and vice versa.
//
simple GenericTrafGen like ITrafGen
{
    parameters:
        @display("i=block/source");

        // generate RawPackets (if true), or plain cPackets (if false) 
        bool generateRaw = default(false);
        // time of sending the first packet
        volatile double startTime @unit("s") = default(1s); 
        // time of finishing sending, negative values mean forever
        volatile double stopTime @unit("s") = default(-1s);  
        // may be a random value, e.g. exponential(1)
        volatile double sendInterval @unit("s") = default(10ms); 
        // max number of packets to generate, -1 means forever
        int numPackets = default(-1); 
        // packet length in bytes
        volatile int packetLength @unit("B"); 
        
        // jitter is only effective if sendInterval > 1E-18s
        // brown noise jitter, standard deviation. 0 = off
        double jitterStddev = default(0);
        // brown noise jitter, lower limit. should be centered around 0
        double jitterMin = default(0);
        // brown noise jitter, upper limit. should be centered around 0
        double jitterMax = default(0);
        // multiplicative jitter term. increases or decreases strength relative to jitterAdditive
        double jitterMultiplicative = default(1);
        // additive jitter term, in Hz! note: added to 1/sendInterval to permit direct adjustments of the sending rate
        double jitterAdditive @unit("Hz") = default(0);
        
        // variables which are set during operation and may be used within startTime, stopTime and sendInterval computations
        // cycle counter, incremented each time stopTime has been reached. updated before startTime is evaluated again
        int cycle = 0;
        // start time of the current cycle, adjusted when startTime has been reached
        double cycleStart @unit("s") = 0s;
        // random number chosen once per instance
        double randVal = uniform(0, 1);

        @signal[sentPk](type=cPacket);
        @signal[rcvdPk](type=cPacket);
        @statistic[rcvdPk](title="packets received"; source=rcvdPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[sentPk](title="packets sent"; source=sentPk; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
        @statistic[endToEndDelay](title="end-to-end delay"; source="messageAge(rcvdPk)"; unit=s; record=histogram,vector; interpolationmode=none);

    gates:
        inout io;
}
