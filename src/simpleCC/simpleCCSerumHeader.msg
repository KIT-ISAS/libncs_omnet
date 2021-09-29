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
}}

namespace inet;

class noncobject SerumRecord;


enum simpleCCSerumDataset {
    DATASET_simpleCC_PUSH = 150; // unused
    DATASET_simpleCC_RESP = 151;
}


// simpleCC
class simpleCCRequestRecord extends SerumRecord {
    type = MONITORING_TO_DST;
    dataDesc = DATASET_simpleCC_RESP;
    
    length = 1; // type plus descriptor in one byte
}

class simpleCCResponseRecord extends SerumRecord {
    type = MONITORING_HH_APPEND;
    dataDesc = DATASET_simpleCC_RESP;
    
    length = 5; // type plus descriptor in 1 byte
    
    // general monitoring
    long lineRate; // 1 byte
    int queueSize; // 1 byte
    double avgQueueLength; // 1 byte
    double avgUtilization; // 1 byte
}
