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

package libncs_omnet.OracleCC;

//
// OracleCC
//
// Implements the same numerical approach as CoCC,
// but with a centralized instance having access to
// the knowledge of all control and CC instances.
//
// This is the central coordinator performing all CC computations.
// Exactly one instance must exist and be located at the root of the network.
//
simple OracleCCCoordinator
{
    // target link utilization in percent
    double targetUtilization = default(0.85);
    // CoCC coexistence mode (see CoCCUDPTransport.ned for documentation)
    int coexistenceMode = default(0);
    // acceptable deviation between newton solution and actual data rate
    double newtonPrecision @unit(bps) = default(100bps);
    // minimum time between CC recomputations
    double minUpdateInterval @unit(s) = default(5ms);

    @display("i=block/network2");
}
