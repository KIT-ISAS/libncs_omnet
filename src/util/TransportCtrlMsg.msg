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
    #include <inet/networklayer/contract/NetworkOptions.h>
    
    #include "CoCC/CoCCTranslator.h"
    #include "NcsContext.h"
    
    typedef inet::L3Address L3Address_t;
    typedef inet::NetworkOptionsPtr NetworkOptionsPtr_t;
}};

class noncobject L3Address_t;
class noncobject NetworkOptionsPtr_t;
class noncobject ICoCCTranslatorPtr;
class noncobject NcsContextComponentIndex;

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
    @customize(true);
    L3Address_t srcAddr;
    unsigned short srcPort;
    L3Address_t dstAddr;
    unsigned short dstPort;
    NetworkOptionsPtr_t networkOptions;
}

// Set CoCC translator pointer in transport layer
message TransportSetTranslator {
    L3Address_t dstAddr;
    unsigned short dstPort;
    ICoCCTranslatorPtr translator;
    NcsContextComponentIndex role;
}

// Notify about start of stream
message TransportStreamStartInfo {
    L3Address_t dstAddr;
    unsigned short dstPort;
    simtime_t start;
}

// Notify about stop of stream
message TransportStreamStopInfo {
    L3Address_t dstAddr;
    unsigned short dstPort;
    simtime_t stop;
}
