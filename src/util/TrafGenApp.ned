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

package libncs_omnet.util;

import libncs_omnet.util.ITransportApp;
import libncs_omnet.util.GenericTrafGen;

//
// TrafGenApp wraps a PacketRedirector and a GenericTrafGen instance according
// to the interface definition of ITransportApp. 
//
module TrafGenApp like ITransportApp
{
    @display("i=block/source");
    gates:
        inout transport @labels(TransportCommand);

    submodules:
        redir: PacketRedirector {
            @display("p=184,125");
        }
        gen: GenericTrafGen {
            @display("p=184,42");
        }

    connections:
        gen.io <--> redir.up;
        redir.down <--> transport;
}
