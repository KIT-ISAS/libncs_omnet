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

import inet.applications.contract.IUDPApp;

//
// Transport for Bytestream-Datagrams (RawPackets) via UDP
// The implementation is restricted to one "connection" between two distinct hosts.
// Multiple connections between the same hosts are not supported (yet).
//
simple UDPTransport like IUDPApp
{
    parameters:
        @display("i=block/transport");

    gates:
        // gate for incoming UDP packets
        input udpIn @labels(UDPControlInfo/up);
        // gate for outgoing UDP packets
        output udpOut @labels(UDPControlInfo/down);
        // gate for incoming and outgoing data/transport commands
        inout up @labels(TransportCommand);
}
