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

package libncs_omnet.FCP.serum;

//
// Manages processing of FCP payload in SERUM containers
//
simple FCPSerumHandler {

    double collectionInterval @unit(s) = default(0.1s);

    double averagingWindow = default(0.02);

    double alpha = default(3);

    double utilizationThreshold = default(0.95);

    double minPrice = default(1);

    bool enableItSmoothing = default(false);

    int itHistoryLength = default(10);

    @signal[price](type="double");

    @statistic[price](source=price;title="FCP price";record=stats,vector?;interpolationmode=sample-hold);

    @signal[i_t](type="double");

    @statistic[i_t](source=i_t;title="FCP i(t)";record=stats,vector?;interpolationmode=sample-hold);

    @signal[sum](type="double");

    @statistic[sum](source=sum;title="FCP sum";record=stats,vector?;interpolationmode=sample-hold);

    @signal[rtt](type="double");

    @statistic[rtt](source=rtt;title="FCP rtt";record=stats,vector?;interpolationmode=sample-hold);

    @signal[matchingPrice](type="double");

    @statistic[matchingPrice](source=matchingPrice;title="FCP matchingPrice";record=stats,vector?;interpolationmode=sample-hold);

    @signal[packetSize](type="double");

    @statistic[packetSize](source=packetSize;title="FCP packetSize";record=stats,vector?;interpolationmode=sample-hold);

    @signal[packetValue](type="double");

    @statistic[packetValue](source=packetValue;title="FCP packetValue";record=stats,vector?;interpolationmode=sample-hold);

    @signal[remainingLinkCapacity](type="double");

    @statistic[remainingLinkCapacity](source=remainingLinkCapacity;title="FCP remainingLinkCapacity";record=stats,vector?;interpolationmode=sample-hold);

    @signal[avgQueueLength](type="double");

    @statistic[avgQueueLength](source=avgQueueLength;title="FCP avgQueueLength";record=stats,vector?;interpolationmode=sample-hold);


    @signal[lastPacketsSize](type="double");

    @statistic[lastPacketsSize](source=lastPacketsSize;title="FCP lastPackets size";record=stats,vector?;interpolationmode=sample-hold);


    @signal[avgQM](type="double");

    @statistic[avgQM](source=avgQM;title="FCP avarage QM";record=stats,vector?;interpolationmode=sample-hold);

    @signal[avgBudget](type="double");

    @statistic[avgBudget](source=avgBudget;title="FCP avarage budget";record=stats,vector?;interpolationmode=sample-hold);

    @signal[utilization](type="double");

    @statistic[utilization](source=utilization;title="Utilization of the link";record=stats,vector?;interpolationmode=sample-hold);

    @signal[packetArrived](type="bool");

    @statistic[packetArrived](source=packetArrived;title="Push Packet Arrived";record=stats,vector?;interpolationmode=sample-hold);

    @signal[oldestPacketDiff](type="double");

    @statistic[oldestPacketDiff](source=oldestPacketDiff;title="FCP serum oldest packet";record=stats,vector?;interpolationmode=sample-hold);


    @signal[originalQM](type="double");

    @statistic[originalQM](source=originalQM;title="FCP average QM";record=stats,vector?;interpolationmode=sample-hold);

    @signal[scaledQM](type="double");

    @statistic[scaledQM](source=scaledQM;title="FCP average QM";record=stats,vector?;interpolationmode=sample-hold);
}
