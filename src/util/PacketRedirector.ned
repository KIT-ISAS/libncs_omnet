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
// Forwards Packets from "up" (source) to "down" (transport) and vice versa, 
// attaching TransportDataMsg as ControlInfo and establishing a connection 
// context if desired.
//
simple PacketRedirector
{
    parameters:
        // The address or hostname of the host to which a connection should be established
        string connectAddress;
        // The port to which the connection should be made.
        // The redirector will not open a connection if the port number is less than zero.
        int connectPort = default(1000);
        // The local port to which the PacketRedirector will listen for incoming connections.
        // The redirector will not listen if the port number is less than zero.
        int localPort = default(1000);  // local port
        // The time at which the connection is established
        double tOpen @unit(s) = default(0s);
        // The time at which the connection is closed. Negative values == never.
        // Note: The connection is actually never closed, but no more messages will be forwarded after this point in time.
        double tClose @unit(s) = default(-1s);

        @display("i=block/classifier");
    gates:
        inout up;
        inout down @labels(TransportCommand);
}
