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


enum CoCCSerumDataset {
    DATASET_COCC_PUSH = 50;
    DATASET_COCC_RESP = 51;
}


class CoCCRequestRecord extends SerumRecord {
    type = MONITORING_TO_DST;
    dataDesc = DATASET_COCC_RESP;
    
    length = 1; // type plus descriptor in one byte
}

class CoCCResponseRecord extends SerumRecord {
    type = MONITORING_HH_APPEND;
    dataDesc = DATASET_COCC_RESP;
    
    length = 10;            // type plus descriptor in 1 byte
    
    // CoCC
    short period;           // 1 byte
    double m_sum;           // 2 byte
    double b_sum;           // 2 byte
    double qm_thresh;       // 2 byte
    double ctrlRate;        // 2 byte
    // debug/info only, not used by CoCC - thus not counted
    long lineRate;
    int flows;
    double avgQueueLength;
    double avgUtilization;
}

class CoCCPushRecord extends SerumRecord {
    type = MONITORING_HH_INLINE;
    dataDesc = DATASET_COCC_PUSH;
    
    length = 12;                // type plus descriptor in one byte
    
    short period;               // 1 byte
    double m;                   // 2 byte
    double b;                   // 2 byte
    double qm_target;           // 2 byte
    double qmDesiredRate;       // 2 byte
    double lastFallbackRate;    // 2 byte
}
