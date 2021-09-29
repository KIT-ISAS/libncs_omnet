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

package libncs_omnet.simpleCC;

import inet.applications.contract.IUDPApp;

//
// Transport for Bytestream-Datagrams (RawPackets) via UDP with simpleCC
// The implementation is restricted to one "connection" between two distinct hosts.
// Multiple connections between the same hosts are not supported (yet).
//
simple simpleCCUDPTransport like IUDPApp
{
    parameters:
        @display("i=block/transport");
        
        @signal[expectedRate](type="double");
        
        @statistic[simpleCCExpectedRate](source=expectedRate;title="simpleCC bitrate expected for QMtarget";record=stats?,vector?;interpolationmode=sample-hold);
        
        // generate an empty packet on monitoring request to return a monitoring reply?
        bool forceMonitoringReply = default(false);
        
        // monitoring push/collection interval
        double collectionInterval @unit(s) = default(0.05s);
        // range within which simpleCC might defer push messages to increase their spread
        double pushSpread @unit(s) = default(0.03s);
        // latest time after which simpleCC will force a push/collection message without waiting for a piggyback opportunity
        double forcedPushDelay @unit(s) = default(0.04s);
        // range within which simpleCC may prematurely start a forced push to increase the spread
        double forcedPushSpread @unit(s) = default(0.009s);
        
        // Overhead per packet induced by lower layers
        int lowerLayerOverhead @unit(B) = default(55Byte); // 7 Byte PPP + 40 Byte IPv6 + 8 Byte UDP
        // Overhead per packet for metadata attached to them
        int metadataOverhead @unit(B) = default(16Byte); // TODO: needs to be verified  
		//new simpleCC 
		double simpleCC_QUEUE_THRESHOLD = default(0.1);
		double simpleCC_CONGESTION_THRESHOLD = default(0.99);
    	double simpleCC_FULL_LOAD_THRESHOLD = default(0.8);
    	double simpleCC_HEAVY_CONGESTION_MULT = default(1.5);
    	double  simpleCC_LIGHT_CONGESTION_MULT = default(1.25);
    	double simpleCC_SLOW_START_THRESHOLD_MULT = default(0.5);
    	double simpleCC_QM_STEP = default(0.01);
		bool simpleCC_enable_random_fullLoadIncrease = default(false);
		bool simpleCC_enable_dynamic_maxDrop = default(false);
		bool simpleCC_enable_weigthed_slowStartTH = default(false);
		double simpleCC_c = default(0.4);
		double simpleCC_beta = default(0.8);
		double simpleCC_RANDOM_MIN_QUEUE_THRESHOLD = default(0.2);
		double simpleCC_RANDOM_MAX_QUEUE_THRESHOLD = default(2.0);
		double simpleCC_DYNAMIC_QUEUE_THRESHOLD_SCALING = default(1.5);
    gates:
        // gate for incoming UDP packets
        input udpIn @labels(UDPControlInfo/up);
        // gate for outgoing UDP packets
        output udpOut @labels(UDPControlInfo/down);
        // gate for incoming and outgoing data/transport commands
        inout up @labels(TransportCommand);
}
