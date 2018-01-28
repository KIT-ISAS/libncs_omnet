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

//
// CtrlInfo to exchange Commands between a SocketWrapper and its user
//

cplusplus {{
    #include <inet/networklayer/common/L3Address.h>
    
    typedef inet::L3Address L3Address_t;
}};

class noncobject L3Address_t;

// Request to establish a connection with a remote host
message TransportConnectReq {
    L3Address_t dstAddr;
    unsigned short dstPort;
}

// Request to start listening
message TransportListenReq {
    unsigned short listenPort;    
}

// Metadata to describe from and to which host a chunk of data should be sent
message TransportDataInfo {
    L3Address_t srcAddr;
    unsigned short srcPort;
    L3Address_t dstAddr;
    unsigned short dstPort;
}
