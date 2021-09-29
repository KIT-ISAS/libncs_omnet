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

import inet.transportlayer.contract.ITCP;
import inet.transportlayer.contract.IUDP;
import libncs_omnet.FCP.contract.IFCP;
import libncs_omnet.util.GenericTransportHost;
import libncs_omnet.util.ITransportApp;
import libncs_omnet.util.TCPTransport;
import libncs_omnet.util.UDPTransport;
import libncs_omnet.util.FCPTransport;

//
// A host configuration module providing transport services of the *Transport
// adapter family to a single application.
// Note: The application can either use one of the supported transports, so
// please ensure you do not enable more than one of them.
//
module AppTransportHost extends GenericTransportHost
{
    parameters:
        @display("bgl=2;i=block/telnet;bgb=,-");

        // The name of the application to run on this host instance.
        string transportAppType;

    gates:

    submodules:
        app: <transportAppType> like ITransportApp {
            @display("p=329,58;i=block/plug");
        }

    connections:
        tcpTransport.up <--> app.transport if useTcp;
        udpTransport.up <--> app.transport if useUdp;
        fcpTransport.up <--> app.transport if useFcp;
}
