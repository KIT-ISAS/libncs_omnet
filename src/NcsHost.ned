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

package libncs_omnet;

import inet.node.inet.NodeBase;
import inet.transportlayer.contract.ITCP;
import inet.transportlayer.contract.IUDP;
import libncs_omnet.FCP.contract.IFCP;
import libncs_omnet.CoCC.CoCCUDPTransport;
import libncs_omnet.OracleCC.OracleCCUDPTransport;
import libncs_omnet.simpleCC.simpleCCUDPTransport;
import libncs_omnet.util.GenericTransportHost;
import libncs_omnet.util.TCPTransport;
import libncs_omnet.util.UDPTransport;
import libncs_omnet.util.FCPTransport;


module NcsHost extends GenericTransportHost
{
    parameters:
        @display("bgl=2;i=device/device;bgb=791,469");

        bool useCoCC = default(false);
        bool useOracleCC = default(false);
        bool useSimpleCC = default(false);

        networkLayer.serumSupportEnabled = default(useCoCC || useFcp || useOracleCC || useSimpleCC);

    gates:
        inout context;

    types:

    submodules:
        cps: NcsCpsApp {
            @display("p=329,50");
        }

        coCC: CoCCUDPTransport if useCoCC {
            @display("p=536,142");
        }

        oracleCC: OracleCCUDPTransport if useOracleCC {
            @display("p=632,142");
        }
        simpleCC: simpleCCUDPTransport if useSimpleCC {
            @display("p=738,142");
        }

    connections:
        tcpTransport.up <--> cps.transport if useTcp;
        fcpTransport.up <--> cps.transport if useFcp;
        udpTransport.up <--> cps.transport if useUdp && !useCoCC && !useOracleCC && !useSimpleCC;
        coCC.up <--> cps.transport if useUdp && useCoCC;
        coCC.udpIn <-- udp.appOut++ if useUdp && useCoCC;
        coCC.udpOut --> udp.appIn++ if useUdp && useCoCC;
        oracleCC.up <--> cps.transport if useUdp && useOracleCC;
        oracleCC.udpIn <-- udp.appOut++ if useUdp && useOracleCC;
        oracleCC.udpOut --> udp.appIn++ if useUdp && useOracleCC;
        simpleCC.up <--> cps.transport if useUdp && useSimpleCC;
        simpleCC.udpIn <-- udp.appOut++ if useUdp && useSimpleCC;
        simpleCC.udpOut --> udp.appIn++ if useUdp && useSimpleCC;

        cps.context <--> context;

}
