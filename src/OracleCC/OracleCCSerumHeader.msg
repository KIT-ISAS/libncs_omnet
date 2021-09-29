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

cplusplus {{
#include "inet/networklayer/serum/SerumHeader_m.h"
#include "OracleCC/OracleCCUDPTransport.h"

typedef OracleCCUDPTransport *OracleCCUDPTransport_Ptr;
typedef void *OracleCC_Handle_Ptr;
}}

namespace inet;

class noncobject SerumRecord;
class noncobject OracleCCUDPTransport_Ptr;
class noncobject OracleCC_Handle_Ptr;


enum OracleCCSerumDataset {
    DATASET_OCC_DISCOVER_INBOUND = 70;
    DATASET_OCC_DISCOVER_OUTBOUND = 71;
}


class OracleCCDiscoverRecord extends SerumRecord {
    type = MONITORING_HH_INLINE;
    dataDesc = DATASET_OCC_DISCOVER_INBOUND; // may also be set to outbound
    
    length = 1; // type plus descriptor in one byte
    OracleCCUDPTransport_Ptr src = nullptr;
    OracleCC_Handle_Ptr handle = nullptr;
}
