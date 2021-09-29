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

package libncs_omnet.OracleCC;

import inet.applications.contract.IUDPApp;

//
// Transport for Bytestream-Datagrams (RawPackets) via UDP with OracleCC
// The implementation is restricted to one "connection" between two distinct hosts.
// Multiple connections between the same hosts are not supported (yet).
//
simple OracleCCUDPTransport like IUDPApp
{
    parameters:
        @display("i=block/transport");
        
        @signal[expectedRate](type="double");
        @signal[appliedQM](type="double");
        
        @statistic[occExpectedRate](source=expectedRate;title="OracleCC bitrate expected for QMtarget";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[occAppliedQM](source=appliedQM;title="OracleCC applied QM";record=stats?,vector?;interpolationmode=sample-hold);

        // Overhead per packet induced by lower layers
        int lowerLayerOverhead @unit(B) = default(55Byte); // 7 Byte PPP + 40 Byte IPv6 + 8 Byte UDP
                
        // configures traffic differentiation / coexistence mode.
        //  0 = disabled, OracleCC does not care about non-control traffic
        //  1 = controlled cooperation: OracleCC still labels all traffic as priorized, 
        //                              but reduces sending rate if targetUtilization is exceeded due to non-control traffic and QMtarget>QMdesired
        //  2 = reactive cooperation: OracleCC labels all traffic for QMtarget>QMdesired as lower-than-best-effort.
        //                            thus, non-control-traffic can suppress this fraction immediately. 
        //                            OracleCC reactively adjusts to the free capacity 
        int coexistenceMode = default(0);
        // sets the desired QM value. Control traffic may be priorized up to this QM value depending on the coexistenceMode
        double qmDesired = default(1);  

    gates:
        // gate for incoming UDP packets
        input udpIn @labels(UDPControlInfo/up);
        // gate for outgoing UDP packets
        output udpOut @labels(UDPControlInfo/down);
        // gate for incoming and outgoing data/transport commands
        inout up @labels(TransportCommand);
}

