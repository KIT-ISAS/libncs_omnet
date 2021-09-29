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
        
        //
        // NcsContext configuration
        //
        
        // Polling interval used to determine wether the OMNeT++/INET networking 
        // stack has finished initialization. A positive value will be required
        // if hosts do get their IP addresses pre-assigned during initialization.
        // NcsContext will try to initiate connections between NCS hosts at the 
        // given polling interval.
        double networkStartupPollInterval @unit(s);
        // Delay before starting to poll the NCS control loop.
        // Allows network components to initialize and establish state/connections 
        // before the NCS starts to communicate.
        double startupDelay @unit(s);
        // Determines what should be done if the controller signals an error condition,
        // e.g. in case the plant is in a not admissible state.
        // 0: ignore (default), 1: stop simulation gracefully, 2: abort simulation with exception
        int actionOnControllerFailure;
        
        //
        // NcsImpl configuration
        //
        // default values are configured to "do not set/change defaults"
        //

        // Absolute or relative path to a file which configures the NCS.
        // See the configured NCS implementation for details.
        string configFile;
        
    gates:
        inout actuator;
        inout controller;
        inout sensor;
}
