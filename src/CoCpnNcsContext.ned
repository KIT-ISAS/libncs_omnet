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

package libncs_omnet;

import libncs_omnet.NcsContext;


simple CoCpnNcsContext extends NcsContext like INcsContext
{
    parameters:
        @class(CoCpnNcsContext);
        
        // default values are configured to "do not set/change MATLAB defaults"
        
        int controlSequenceLength = default(-1);
        int maxControlSequenceDelay = default(-1);
        int maxMeasDelay = default(-1);
        // Since the NCS model depends on the sampling interval, it is most
        // likely suboptimal to change the samplig interval by using the
        // OMNeT simulation configuration, but instead by choosing an accordingly
        // configured .mat file. Unless you hook the MATLAB interface to 
        // re-compute the NCS model according to the new value.
        double samplingInterval = default(-1);
        string scDelayProbs = default("");
        string caDelayProbs = default("");
        
        int controllerEventBased = default(-1); // boolean, 0 = off, >0 on
        double controllerDeadband = default(-1);
        int controllerEventTrigger = default(-1); // 1: QoC, 2: stage costs, 3: inputs only (default)
        int sensorEventBased = default(-1); // boolean, 0 = off, >0 on
        double sensorMeasDelta = default(-1);
}
