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
import libncs_omnet.util.GenericTransportHost;
import libncs_omnet.util.TCPTransport;
import libncs_omnet.util.UDPTransport;


module NcsHost extends GenericTransportHost
{
    parameters:
        @display("bgl=2;i=device/device;bgb=634,469");

    gates:
        inout context;

    submodules:
        cps: NcsCpsApp {
            @display("p=329,69");
        }

    connections:
        tcpTransport.up <--> cps.transport if useTcp;
        udpTransport.up <--> cps.transport if useUdp;

        cps.context <--> context;

}
