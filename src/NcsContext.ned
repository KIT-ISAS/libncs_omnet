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
// NcsContext represents the state and behavior of a single NCS.
// It conveys packets sent from the NCS model to the network simulation domain 
// and vice versa.
// The actual NCS type is configurable, a realization of AbstractNcsImpl 
// performs the actual modeling of an NCS, or acts as a proxy, e.g. to a MATLAB
// model.
//
simple NcsContext like INcsContext
{
    parameters:
        @display("i=block/cogwheel");
        
        //
        // NcsContext configuration
        //
        
        // name of the NCS implementation to use
        string ncsImpl = default("libncs_omnet.MatlabImpl.MatlabNcsImpl");
        
        // Polling interval used to determine wether the OMNeT++/INET networking 
        // stack has finished initialization. A positive value will be required
        // if hosts do get their IP addresses pre-assigned during initialization.
        // NcsContext will try to initiate connections between NCS hosts at the 
        // given polling interval.
        double networkStartupPollInterval @unit(s) = default(0s);
        // Delay before starting to poll the NCS control loop.
        // Allows network components to initialize and establish state/connections 
        // before the NCS starts to communicate.
        double startupDelay @unit(s) = default(0s);
        // Time for which the NCS should be simulated. 
        // NCS simulation will be stopped at startupDelay + simulationRuntime.
        // 0 == unlimited runtime
        double simulationRuntime @unit(s) = default(0s);
        // Determines what should be done if the plant/controller simulation signals an error condition,
        // e.g. in case the plant is in a not admissible state.
        // 0: ignore (default), 1: stop simulation gracefully, 2: abort simulation with exception
        int actionOnControllerFailure = default(1);
        
        // Sampling of communication latencies
        // minimum number of samples used for histogram
        // (will not be pruned even if outdated)
        int minSampleCount = default(25);
        // maximum number of samples used for histogram
        // (will prune oldes samples even if not outdated)
        int maxSampleCount = default(100);
        // sample age after which samples get pruned
        double maxSampleAge @unit("s") = default(0.25s);
        // number of sample bins, each spaced in control sampling rate intervals
        int maxSampleBins = default(10);
        // report time steps in which the controller did not send a control sequence
        // as lost packet? useful for event-triggered communication to account for
        // packets which were suppressed without knowledge of the controller
        bool reportUnusedStepsAsLoss = default(false);
        // point in time at which pkt statistics are reset (for evaluation purposes)
        double pktStatisticsStartDelay @unit(s) = default(0s);
        
        //
        // NcsImpl configuration
        //
        // default values are configured to "do not set/change defaults"
        //
        
        // Absolute or relative path to a file which configures the NCS.
        // See the configured NCS implementation for details.
        string configFile;
        // select one of multiple controller implementations, if supported
        // empty: use default of loaded configuration
        string controllerClassName = default("");
        
        // select one of the filter implementations, if supported
        // empty: use default of loaded configuration
        string filterClassName = default("");
        
        // select the network type
        // -1: default of loaded configuration, 1: TcpLike, 2: UdpLike, 3: UdpLike with Acks (default)
        int networkType = default(-1); 

        // Length of control sequence sent to actuator
        // -1: default of loaded configuration
        volatile int controlSequenceLength = default(-1);
        // Maximum delay (in time steps) which sensor measurments should expect to experience
        // -1: default of loaded configuration
        int maxMeasDelay = default(-1);
        
        // Time horizon (in time steps) considered for MPC controllers
        // -1: default of loaded configuration
        int mpcHorizon = default(-1);
        
        // Averaging window size (in seconds) used to determine the actual control error
        // -1: default of loaded configuration
        double controlErrorWindowSize = default(-1);
                        
        //
        // Signals and statistics
        //
        
        // Generated in OMNeT
        
        @signal[sc_delay_act](type="simtime_t"); // network (time of reception)
        @signal[ca_delay_act](type="simtime_t"); // network (time of reception)
        @signal[ac_delay_act](type="simtime_t"); // network (time of reception)
        @statistic[act_delay_sc](source=sc_delay_act;title="Actual Sensor-Controller delay";record=stats?,vector?;interpolationmode=none);
        @statistic[act_delay_ca](source=ca_delay_act;title="Actual Controller-Actuator delay";record=stats?,vector?;interpolationmode=none);
        @statistic[act_delay_ac](source=ac_delay_act;title="Actual Actuator-Controller delay";record=stats?,vector?;interpolationmode=none);
        
        @signal[sc_sent](type="bool");
        @signal[ca_sent](type="bool");
        @signal[ac_sent](type="bool");
        @statistic[meas_sent](source=sc_sent;title="Measurement Sent";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[control_seq_sent](source=ca_sent;title="Control Sequence Sent";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[actuator_ack_sent](source=ac_sent;title="Actuator ACK Sent";record=stats?,vector?;interpolationmode=sample-hold);
        
        @signal[sen_pktsSent_s](type="long");
        @signal[sen_pktsArrived_s](type="long");
        @signal[sen_pktsLost_s](type="long");
        @signal[con_pktsSent_s](type="long");
        @signal[con_pktsArrived_s](type="long");
        @signal[con_pktsLost_s](type="long");
        @signal[act_pktsSent_s](type="long");
        @signal[act_pktsArrived_s](type="long");
        @signal[act_pktsLost_s](type="long");
        @statistic[sen_pktsSent](source=sen_pktsSent_s;title="Packets sent by Sensor";record=last);
        @statistic[sen_pktsArrived](source=sen_pktsArrived_s;title="Packets from Sensor which arrived";record=last);
        @statistic[sen_pktsLost](source=sen_pktsLost_s;title="Packets from Sensor which were lost";record=last);
        @statistic[con_pktsSent](source=con_pktsSent_s;title="Packets sent by Controller";record=last);
        @statistic[con_pktsArrived](source=con_pktsArrived_s;title="Packets from Controller which arrived";record=last);
        @statistic[con_pktsLost](source=con_pktsLost_s;title="Packets from Controller which were lost";record=last);
        @statistic[act_pktsSent](source=act_pktsSent_s;title="Packets sent by Actuator";record=last);
        @statistic[act_pktsArrived](source=act_pktsArrived_s;title="Packets from Actuator which arrived";record=last);
        @statistic[act_pktsLost](source=act_pktsLost_s;title="Packets from Actuator which were lost";record=last);
        
        @signal[controlPeriod_s](type="simtime_t");
        @statistic[controlPeriod](source=controlPeriod_s;title="MATLAB NCS control period";record=stats?,vector?;interpolationmode=sample-hold); 

        // Generated by NcsImpl
        
        @signal[plantStateAdmissible_s](type="bool");
        @signal[controllerStateAdmissible_s](type="bool");
        @signal[act_control_error](type="double");
        @signal[est_control_error](type="double");
        @signal[act_stage_costs](type="double");
        @statistic[plantStateAdmissible](source=plantStateAdmissible_s;title="Plant state was admissible during simulation";record=last);
        @statistic[controllerStateAdmissible](source=controllerStateAdmissible_s;title="Controller state was admissible during simulation";record=last);
        @statistic[actual_control_error](source=act_control_error;title="Actual Control Error";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[estimated_control_error](source=est_control_error;title="Estimated Control Error";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[actual_stage_costs](source=act_stage_costs;title="Actual Stage Costs";record=stats?,vector?;interpolationmode=sample-hold);
        
        @signal[sc_delay_obs](type="simtime_t"); // CPS-CPS (time of processing)
        @signal[ca_delay_obs](type="simtime_t"); // CPS-CPS (time of processing)
        @signal[ac_delay_obs](type="simtime_t"); // CPS-CPS (time of processing)
        @statistic[obs_delay_sc](source=sc_delay_obs;title="Observed Sensor-Controller delay";record=stats?,vector?;interpolationmode=none);
        @statistic[obs_delay_ca](source=ca_delay_obs;title="Observed Controller-Actuator delay";record=stats?,vector?;interpolationmode=none);
        @statistic[obs_delay_ac](source=ac_delay_obs;title="Observed Actuator-Controller delay";record=stats?,vector?;interpolationmode=none);
        
    gates:
        inout actuator;
        inout controller;
        inout sensor;
}
