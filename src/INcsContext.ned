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

//
// A INcsContext implementation represents the state and behavior of a single 
// NCS implemented in MATLAB. It conveys packets sent from the MATLAB model to 
// the network simulation domain and vice versa.
//
moduleinterface INcsContext
{
    parameters:
        @display("i=block/cogwheel");
        
        // Delay before starting to poll the NCS control loop.
        // Allows network components to initialize and establish state/connections 
        // before the NCS starts to communicate.
        double startupDelay @unit(s);
        // Absolute or relative path to a file which configures the NCS in MATLAB.
        // This might be a .mat file or something else which is parsed by the MATLAB model.
        string configFile;
    gates:
        inout actuator;
        inout controller;
        inout sensor;
}
