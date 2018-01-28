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

import inet.node.inet.NodeBase;
import inet.transportlayer.contract.ITCP;
import inet.transportlayer.contract.IUDP;
import libncs_omnet.util.ITransportApp;
import libncs_omnet.util.TCPTransport;
import libncs_omnet.util.UDPTransport;

//
// A host configuration module providing transport services of the *Transport
// adapter family.
//
module GenericTransportHost extends NodeBase
{
    parameters:
        @display("bgl=2");

        // enables TCPTransport support
        bool useTcp = default(true);
        // enables UDPTransport support
        bool useUdp = default(false);

        string tcpType = default(firstAvailableOrEmpty("TCP", "TCP_lwIP", "TCP_NSC"));  // tcp implementation (e.g. ~TCP, ~TCP_lwIP, ~TCP_NSC) or ~TCPSpoof
        string udpType = default(firstAvailableOrEmpty("UDP"));
        forwarding = default(false);  // disable routing by default
        networkLayer.proxyARP = default(false);

    gates:

    submodules:
        tcp: <tcpType> like ITCP if useTcp {
            @display("p=287,219");
        }
        udp: <udpType> like IUDP if useUdp {
            parameters:
                @display("p=371,219");
        }
        tcpTransport: TCPTransport if useTcp {
            @display("p=287,142");
        }
        udpTransport: UDPTransport if useUdp {
            @display("p=371,142");
        }
        
    connections:
        tcp.ipIn <-- networkLayer.transportOut++ if useTcp;
        tcp.ipOut --> networkLayer.transportIn++ if useTcp;

        udp.ipOut --> networkLayer.transportIn++ if useUdp;
        udp.ipIn <-- networkLayer.transportOut++ if useUdp;

        tcpTransport.tcpIn <-- tcp.appOut++ if useTcp;
        tcpTransport.tcpOut --> tcp.appIn++ if useTcp;
        udpTransport.udpIn <-- udp.appOut++ if useUdp;
        udpTransport.udpOut --> udp.appIn++ if useUdp;
}
