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

#include "CoCpnNcsContext.h"
#include <omnetpp/cstringtokenizer.h>

Define_Module(CoCpnNcsContext);

const char CoCpnNcsContext::CTRL_SEQ_LEN[] = "controlSequenceLength";
const char CoCpnNcsContext::MAX_CTRL_SEQ_DELAY[] = "maxControlSequenceDelay";
const char CoCpnNcsContext::MAX_MEAS_DELAY[] = "maxMeasDelay";
const char CoCpnNcsContext::SAMPL_INTERVAL[] = "samplingInterval";
const char CoCpnNcsContext::SC_DELAY_PROBS[] = "scDelayProbs";
const char CoCpnNcsContext::CA_DELAY_PROBS[] = "caDelayProbs";

std::vector<const char *> CoCpnNcsContext::getConfigFieldNames() {
    std::vector<const char *> result = NcsContext::getConfigFieldNames();

    testNonnegLong(result, CTRL_SEQ_LEN);
    testNonnegLong(result, MAX_CTRL_SEQ_DELAY);
    testNonnegLong(result, MAX_MEAS_DELAY);
    testNonnegDbl(result, SAMPL_INTERVAL);
    testNonemptyDblVect(result, SC_DELAY_PROBS);
    testNonemptyDblVect(result, CA_DELAY_PROBS);

    return result;
}

void CoCpnNcsContext::setConfigValues(mwArray &cfgStruct) {
    setNonnegLong(cfgStruct, CTRL_SEQ_LEN);
    setNonnegLong(cfgStruct, MAX_CTRL_SEQ_DELAY);
    setNonnegLong(cfgStruct, MAX_MEAS_DELAY);
    setNonnegDbl(cfgStruct, SAMPL_INTERVAL);
    setNonemptyDblVect(cfgStruct, SC_DELAY_PROBS);
    setNonemptyDblVect(cfgStruct, CA_DELAY_PROBS);
}
