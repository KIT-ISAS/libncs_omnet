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
// along with this program. If not, see http://www.gnu.org/licenses/.
// 

package libncs_omnet.util;

import inet.applications.contract.ITCPApp;

//
// Transport for Bytestream-Data or Datagram-Data (RawPackets) via TCP
// The implementation is restricted to one "connection" between two distinct hosts.
// Multiple connections between the same hosts are not supported (yet).
// Note: In datagram mode, the size of a datagram is restricted to at most 2^16-2 byte.
// This might be changed by adjusting the type of PayloadSize_t accordingly in TCPTransport.h
//
simple TCPTransport like ITCPApp
{
    parameters:
        @display("i=block/wheelbarrow");

        // selects if data is forwarded as cPacket objects or as raw bytestream
        bool bytestreamService = default(false);
        // Enables a datagram-style behavior instead of the standard data-stream interface.
        // If datagramService AND bytestreamService is set to true, a small additional header is wrapped around each chunk of data.
        bool datagramService = default(false);

    gates:
        // gate for incoming TCP packets
        input tcpIn @labels(TCPCommand/up);
        // gate for outgoing TCP packets
        output tcpOut @labels(TCPCommand/down);
        // gate for incoming and outgoing data/transport commands
        inout up @labels(TransportCommand);
}
