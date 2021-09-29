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

import inet.applications.contract.IUDPApp;

//
// Transport for Bytestream-Datagrams (RawPackets) via UDP with CoCC
// The implementation is restricted to one "connection" between two distinct hosts.
// Multiple connections between the same hosts are not supported (yet).
//
simple CoCCUDPTransport like IUDPApp
{
    parameters:
        @display("i=block/transport");
        
        @signal[expectedRate](type="double");
        @signal[rateLimitDrop](type="long");
        @signal[reportedM](type="double");
        @signal[reportedB](type="double");
        @signal[reportedQMtarget](type="double");
        @signal[lastFallbackRate](type="double");
        @signal[qmDesiredRate](type="double");
        @signal[periodMismatch](type="bool");
        @signal[responseMissed](type="bool");
        @signal[bottleneckIndex](type="long");
        @signal[bottleneckCtrlRate](type="double");
        @signal[bottleneckShare](type="double");
        @signal[bottleneckQM](type="double");
        @signal[appliedQM](type="double");
        @signal[pushForced](type="bool");
        @signal[forcedPushCompensation](type="double");
        
        @statistic[coccExpectedRate](source=expectedRate;title="CoCC bitrate expected for QMtarget";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccRateLimitDrop](source=rateLimitDrop;title="CoCC rate-limit dropped PktBytes";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccReportedM](source=reportedM;title="CoCC reported linearization M";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccReportedB](source=reportedB;title="CoCC reported linearization B";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccReportedQMtarget](source=reportedQMtarget;title="CoCC reported QMtarget";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccLastFallbackRate](source=lastFallbackRate;title="CoCC reported last fallback rate";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccQMDesiredRate](source=qmDesiredRate;title="CoCC reported QM desired rate";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccPeriodMismatch](source=periodMismatch;title="CoCC period mismatch indicator";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccResponseMissed](source=responseMissed;title="CoCC missing push response indicator";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccBottleneckIndex](source=bottleneckIndex;title="CoCC bottleneck index";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccBottleneckCtrlRate](source=bottleneckCtrlRate;title="CoCC bottleneck available control rate";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccBottleneckShare](source=bottleneckShare;title="CoCC bottleneck rate share";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccBottleneckQM](source=bottleneckQM;title="CoCC botteleneck QM";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccAppliedQM](source=appliedQM;title="CoCC applied QM";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccPushForced](source=pushForced;title="CoCC push was forced?";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[coccForcedPushCompensation](source=forcedPushCompensation;title="CoCC overhead compensation factor for forced pushes";record=stats?,vector?;interpolationmode=sample-hold);
                
        // monitoring push/collection interval
        double collectionInterval @unit(s) = default(0.05s);
        // enables bloom-filter-based collection extension in routers
        // this disables special treatment of push messages (no EF queue) and push spreading
        bool enableRobustCollection = default(false);
        
        // communication period scheme
        // | ACK return phase | RTT/2 | push phase | RTT/2 |
        // feedback is commited at the beginning of the push phase
        // ACK/push spreading scheme
        // phase start >| randomized push | forced push |< phase end
        bool enablePushSpreading = default(!enableRobustCollection);
        bool autoSpreading = default(false); // increase forced push fraction to compensate for low regular push probability
        double ackFraction = default(0.2); // ACK return phase = (collectionInterval - RTT) * ackFraction 
        double maxPushPathDelay @unit(s) = default(0.01s); // maximum expected one-way path delay, 0 <= 2x value < collectionInterval
        double regularPushFraction = default(0.85); // fraction of available phase in which push spreading occurs, 0 <= value <= 1
        double forcedPushFraction = default(0.1); // fraction of available phase in which push will be forced, 0 <= value <= (1 - regularPushFraction)
        
                
        // enables continous adjustment of QM_target between collection intervals
        bool continousQMAdjustment = default(true);
        // enables compensation for rate prediction errors caused by linearization
        bool compensateLinearizationErrors = default(true);
        // sets the time horizon for estimating the overhead due to forced push. -1 = off
        double pushCompensationHorizon @unit(s) = default(1s);
        
        // enables token bucket meter which constraints the burstiness of control traffic
        bool enableRateLimiting = default(false);
        // Overhead per packet induced by lower layers
        int lowerLayerOverhead @unit(B) = default(55Byte); // 7 Byte PPP + 40 Byte IPv6 + 8 Byte UDP
        // Overhead per packet for metadata attached to them
        int metadataOverhead @unit(B) = default(24Byte);  
        // Size of an empty forced push packet
        int forcedPushSize @unit(B) = default(79B);
        // Burst size permitted before rate limit is enforced
        int permittedBurstSize @unit(B) = default(1500B); 
        
        // configures traffic differentiation / coexistence mode.
        //  0 = disabled, CoCC does not care about non-control traffic
        //  1 = controlled cooperation: CoCC still labels all traffic as priorized, 
        //                              but reduces sending rate if targetUtilization is exceeded due to non-control traffic and QMtarget>QMdesired
        //  2 = reactive cooperation: CoCC labels all traffic for QMtarget>QMdesired as lower-than-best-effort.
        //                            thus, non-control-traffic can suppress this fraction immediately. 
        //                            CoCC reactively adjusts to the free capacity
        //  3 = submission: CoCC labels no traffic (except push messages) as priorized
        //  4 = total submission: CoCC labels absolutely no traffic as priorized 
        int coexistenceMode = default(0);
        // sets the desired QM value. Control traffic may be priorized up to this QM value depending on the coexistenceMode
        double qmDesired = default(1);  

    gates:
        // gate for incoming UDP packets
        input udpIn @labels(UDPControlInfo/up);
        // gate for outgoing UDP packets
        output udpOut @labels(UDPControlInfo/down);
        // gate for incoming and outgoing data/transport commands
        inout up @labels(TransportCommand);
}
