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

import libncs_omnet.util.ITransportApp;

// Adapter to translate NCS messages to transport layer API. 
// Represents (the application layer of) a logical CPS.
// Maintains and accepts an arbitrary number of open transport connections.
//
// Note: NcsCpsApp is designed for one open connection per host, since it 
// currently does not keep track of port numbers or connection contexts.
simple NcsCpsApp like ITransportApp {
    parameters:
        @display("i=block/plug");
        
        // If true, this CPS will start to listen at connectPort
        bool doListen = default(false);
        // The port to which this CPS will connect in case of outbound 
        // connection attemps, and which will be used if this CPS listens for 
        // incoming connections
        int connectPort = default(2342);
    gates:
        inout context;
        inout transport @labels(TransportCommand);
}
