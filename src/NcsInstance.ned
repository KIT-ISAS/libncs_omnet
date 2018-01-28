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

module NcsInstance
{
    @display("i=block/rxtx;bgb=397,177");
    string contextType = default("NcsContext");
    gates:
        inout gController @labels(EtherFrame-conn);
        inout gSensor @labels(EtherFrame-conn);
        inout gActor @labels(EtherFrame-conn);
    submodules:
        controller: NcsHost {
            @display("p=62,124");
        }
        sensor: NcsHost {
            @display("p=191,124");
            cps.doListen = default(true);
        }
        actuator: NcsHost {
            @display("p=325,124");
            cps.doListen = default(true);
        }
        ncsContext: <contextType> like INcsContext {
            @display("p=191,40");
        }
    connections:
        sensor.context <--> ncsContext.sensor;
        actuator.context <--> ncsContext.actuator;
        controller.context <--> ncsContext.controller;
        gController <--> { @display("m=s"); } <--> controller.pppg++;
        gSensor <--> { @display("m=s"); } <--> sensor.pppg++;
        gActor <--> { @display("m=s"); } <--> actuator.pppg++;
}
