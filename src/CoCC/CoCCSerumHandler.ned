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

package libncs_omnet.CoCC;

//
// Manages processing of CoCC payload in SERUM containers
//
simple CoCCSerumHandler {
    
    @signal[s_pushDelayed](type="bool");
    @signal[s_pushDuplicate](type="bool");
    @signal[s_mFiltered](type="double");
    @signal[s_bFiltered](type="double");
    @signal[s_mReported](type="double");
    @signal[s_bReported](type="double");
    @signal[s_beRate](type="double");
    @signal[s_qmRate](type="double");
    @signal[s_ctrlRate](type="double");
    @signal[s_qmThresh](type="double");
    @signal[s_numFlows](type="long");
        
    @statistic[coccShPushDelayedDrop](source=s_pushDelayed;title="CoCC SERUM handler dropped delayed push record";record=stats?,vector?;interpolationmode=sample-hold);
    @statistic[coccShPushDuplicateDrop](source=s_pushDuplicate;title="CoCC SERUM handler dropped duplicate push record";record=stats?,vector?;interpolationmode=sample-hold);
    @statistic[coccShmFiltered](source=s_mFiltered;title="CoCC SERUM handler filtered m_sum";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShbFiltered](source=s_bFiltered;title="CoCC SERUM handler filtered b_sum";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShmReported](source=s_mReported;title="CoCC SERUM handler reported m_sum";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShbReported](source=s_bReported;title="CoCC SERUM handler reported b_sum";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShBeRate](source=s_beRate;title="CoCC SERUM handler BE class rate";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShQmRate](source=s_qmRate;title="CoCC SERUM handler qmDesired rate";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShCtrlRate](source=s_ctrlRate;title="CoCC SERUM handler reported ctrlRate";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShQmThresh](source=s_qmThresh;title="CoCC SERUM handler reported qmThresh";record=vector?;interpolationmode=sample-hold);
    @statistic[coccShNumFlows](source=s_numFlows;title="CoCC SERUM handler active flow count";record=vector?;interpolationmode=sample-hold);
    
    
    
    // monitoring push/collection interval
    double collectionInterval @unit(s) = default(0.05s);
    
    // enables bloom-filter-based collection extension
    // keeps a fallback prediction for collection-cycle n+1 which will be used if the push message is lost
    bool enableRobustCollection = default(false);
    int bloomFlowCount = default(1000);
    double bloomErrorRate = default(1e-6);
    
    // target link utilization in percent
    double targetUtilization = default(0.85);
    // CoCC coexistence mode (see CoCCUDPTransport.ned for documentation)
    int coexistenceMode = default(0);
    
    // enables temporary target QoC reduction to cut queuing delays 
    bool enableQueueReduction = default(false);
    // average queue utilization (in pkts) which is acceptable without countermeasures
    double acceptableQueueUtilization = default(1.5);
    // time after which the queue utilization should be reduced to the acceptable threshold
    double queueReductionTime @unit(s) = default(0.2s);
    
    // enables temporary target QoC reduction to constraint rate variations
    bool enableRateControl = default(false);
}
