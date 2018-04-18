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
// NcsContext represents the state and behavior of a single NCS implemented in 
// MATLAB. It conveys packets sent from the MATLAB model to the network 
// simulation domain and vice versa.
//
simple NcsContext like INcsContext
{
    parameters:
        @display("i=block/cogwheel");
        
        @signal[sc_sent](type="bool");
        @signal[ca_sent](type="bool");
        @signal[ac_sent](type="bool");
        @signal[act_qoc](type="double");
        @signal[sc_delay_obs](type="simtime_t"); // CPS-CPS (time of processing)
        @signal[ca_delay_obs](type="simtime_t"); // CPS-CPS (time of processing)
        @signal[ac_delay_obs](type="simtime_t"); // CPS-CPS (time of processing)
        @signal[sc_delay_act](type="simtime_t"); // network (time of reception)
        @signal[ca_delay_act](type="simtime_t"); // network (time of reception)
        @signal[ac_delay_act](type="simtime_t"); // network (time of reception)
        
        @statistic[meas_sent](source=sc_sent;title="Measurement Sent";record=stats,vector?;interpolationmode=sample-hold);
        @statistic[control_seq_sent](source=ca_sent;title="Control Sequence Sent";record=stats,vector?;interpolationmode=sample-hold);
        @statistic[actuator_ack_sent](source=ac_sent;title="Actuator ACK Sent";record=stats,vector?;interpolationmode=sample-hold);
        @statistic[actual_qoc](source=act_qoc;title="Actual QoC";record=stats,vector?;interpolationmode=sample-hold);
        @statistic[obs_delay_sc](source=sc_delay_obs;title="Observed Sensor-Controller delay";record=stats,histogram?,vector?;interpolationmode=none);
        @statistic[obs_delay_ca](source=ca_delay_obs;title="Observed Controller-Actuator delay";record=stats,histogram?,vector?;interpolationmode=none);
        @statistic[obs_delay_ac](source=ac_delay_obs;title="Observed Actuator-Controller delay";record=stats,histogram?,vector?;interpolationmode=none);
        @statistic[act_delay_sc](source=sc_delay_act;title="Actual Sensor-Controller delay";record=stats,histogram?,vector?;interpolationmode=none);
        @statistic[act_delay_ca](source=ca_delay_act;title="Actual Controller-Actuator delay";record=stats,histogram?,vector?;interpolationmode=none);
        @statistic[act_delay_ac](source=ac_delay_act;title="Actual Actuator-Controller delay";record=stats,histogram?,vector?;interpolationmode=none);
        
        // Delay before starting to poll the NCS control loop.
        // Allows network components to initialize and establish state/connections 
        // before the NCS starts to communicate.
        double startupDelay @unit(s) = default(0s);
        // Absolute or relative path to a file which configures the NCS in MATLAB.
        // This might be a .mat file or something else which is parsed by the MATLAB model.
        string configFile;
    gates:
        inout actuator;
        inout controller;
        inout sensor;
}
