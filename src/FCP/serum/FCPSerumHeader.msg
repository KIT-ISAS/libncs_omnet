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


enum FCPSerumDataset {
    DATASET_FCP_PUSH = 42;
    DATASET_FCP_RESP = 43;
}


class FCPPushRecord extends SerumRecord {
    type = MONITORING_HH_INLINE;
    dataDesc = DATASET_FCP_PUSH;

    length = 20;

    double price;       // 4B
    double rtt;         // 2B
    double preload;     // 0B
    int size;           // 2B

    bool collectQM;     // 1B
    double bottleneckQM;// 2B
    double targetQM;    // 2B

    double budget;      // 4B
    int flows;          // 2B, for debugging
}

class FCPResponseRecord extends SerumRecord {
    type = MONITORING_HH_APPEND;
    dataDesc = DATASET_FCP_RESP;

    length = 7;

    double prevAvgBottleneckQM;     // 2B
    double averageBottleneckQM;     // 2B
    double averageBottleneckBudget; // 4B
    int flows;                      // 2B
}
