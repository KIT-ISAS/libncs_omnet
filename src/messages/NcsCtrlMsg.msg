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
// CtrlInfo for request from MatLab to establish a connection between 
// Controller and Sensor/Actuator
//

cplusplus {{
    #include <inet/networklayer/common/L3Address.h>
    
    #include "CoCC/CoCCTranslator.h"
    #include "NcsContext.h"
    
    typedef inet::L3Address L3Address_t;
}};

class noncobject L3Address_t;
class noncobject ICoCCTranslatorPtr;
class noncobject NcsContextComponentIndex;

// Connection request of a CPS within a NCS
message NcsConnReq {
    L3Address_t dstAddr;
}

// CtrlInfo passed along with data in a RawPacket
message NcsSendData {
    L3Address_t srcAddr;
    L3Address_t dstAddr;
}

// Set CoCC translator pointer in transport layer
message NcsSetTranslator {
    L3Address_t dstAddr;
    ICoCCTranslatorPtr translator;
    NcsContextComponentIndex role;
}

// Notify about start of stream
message NcsStreamStartInfo {
    L3Address_t dstAddr;
    simtime_t start;
}

// Notify about stop of stream
message NcsStreamStopInfo {
    L3Address_t dstAddr;
    simtime_t stop;
}
