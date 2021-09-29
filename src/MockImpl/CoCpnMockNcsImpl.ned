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

package libncs_omnet.MockImpl;

//
// Mock implementation of an NCS in OMNeT++ with support for
// enhanced information exchange between control and communication
//
simple CoCpnMockNcsImpl
{
    parameters:
        
        @class(CoCpnMockNcsImpl);
        @display("i=block/segm");
    
        @signal[mockPredictedPktUtility](type="double");
        @signal[mockPredictedPktUtilityVariance](type="double");
        @signal[mockShortPredictedPktUtility](type="double");
        @signal[mockShortPredictedPktUtilityVariance](type="double");
        @signal[mockPktUtility](type="double");
        @signal[mockUnscaledPktUtility](type="double");
        @signal[mockPredictedQoC](type="double");
        @signal[mockAvgQoC](type="double");
        @signal[mockActQoC](type="double");
        @signal[mockAvgRate](type="double");
        @signal[mockActRate](type="double");
        @signal[mockRateAccumulator](type="double");
        @signal[mockRatePhaseShift](type="double");
        
        @statistic[predictedPktUtility](source=mockPredictedPktUtility;title="Predicted Unscaled Packet Utility";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[predictedPktUtilityVariance](source=mockPredictedPktUtilityVariance;title="Predicted Unscaled Packet Utility Variance";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[shortPredictedPktUtility](source=mockShortPredictedPktUtility;title="Short Term Predicted Unscaled Packet Utility";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[shortPredictedPktUtilityVariance](source=mockShortPredictedPktUtilityVariance;title="Short Term Predicted Unscaled Packet Utility Variance";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[pktUtility](source=mockPktUtility;title="Weighted Packet Utility";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[unscaledPktUtility](source=mockUnscaledPktUtility;title="Unscaled Packet Utility";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[predictedQoC](source=mockPredictedQoC;title="Predicted QoC";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[avgQoC](source=mockAvgQoC;title="Average QoC";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[actQoC](source=mockActQoC;title="Actual QoC";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[avgRate](source=mockAvgRate;title="Average Packet Rate";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[actRate](source=mockActRate;title="Actual Packet Rate";record=stats?,vector?;interpolationmode=sample-hold);
        @statistic[rateAccumulator](source=mockRateAccumulator;title="Rate Accumulator Value";record=stats,vector?;interpolationmode=sample-hold);
        @statistic[ratePhaseShift](source=mockRatePhaseShift;title="Rate Phase Shift";record=stats,vector?;interpolationmode=sample-hold);

        //
        // easy-to-use parameterization
        //
        double tickerFrequency @unit(Hz) = default(100Hz);
        double minPktDelayS @unit(s) = default(10ms);
        double maxPktDelayS @unit(s) = default(50ms);
        double maxPktDelayUtility = default(0.68);
        double historySamplingTime @unit(s) = default(1s);
        double utilitySamplingTime @unit(s) = default(100ms);
        double qocRandomizationInterval @unit(s) = default(70ms);
        double rateRandomizationInterval @unit(s) = default(30ms);
        
        //
        // real parameters as used in model
        //

        // generate RawPackets with dummy content (if true), or with empty data array (if false) 
        bool fillRawPackets = default(false);

        string mockFunction = default("libncs_omnet.MockImpl.LinearMockFunction");
    
        double tickerInterval @unit(s) = default(1.0s / dropUnit(tickerFrequency));
        double tickerDrift = default(1 + uniform(-20e-6, 20e-6)); // +/-20ppm is a usual accuracy for crystal oscillators
        
        int maxPktDelay = default(int(dropUnit(maxPktDelayS) * dropUnit(tickerFrequency)));
        double delayUtilityFactor = default((maxPktDelayUtility - 1.0) / (dropUnit(maxPktDelayS - minPktDelayS) * dropUnit(tickerFrequency)));
        double delayUtilityOffset = default(1.0 - dropUnit(minPktDelayS) * dropUnit(tickerFrequency) * delayUtilityFactor);
        double delayUtilityLimit = default(1.0);
        
        // utility weighting function f(x) = x^a / (b + x^c)
        double a = default(2);
        double b = default(4);
        double c = default(4);
        // utility weighting function distortion g(x) = s x / (t + x)
        // used to compute f(g(x))
        double s = default(10);
        double t = default(1);
        // evaluation of utility weights
        double utilityEvalStep = default(1.5 / historySamplingSteps);
        double utilityOffset = default(0.03);
        
        int historySamplingSteps = default(int(dropUnit(historySamplingTime) * dropUnit(tickerFrequency))); // length of QoC history used to determine actual QoC. samples are weigthed by f(g(x))
        int utilitySamplingSteps = default(min(100,max(10,int(dropUnit(utilitySamplingTime) * dropUnit(tickerFrequency))))); // length of packet utility history used to determine current packet utility.
        
        bool useRatePrediction = default(false);
        int ratePredictionShortWindowSteps = default(int(dropUnit(s(250ms)) * dropUnit(tickerFrequency)));
        int ratePredictionLongWindowSteps = default(int(dropUnit(4s) * dropUnit(tickerFrequency)));
        // threshold for comparison with squared difference between short- and long-term mean
        double ratePredictionTruncationThreshold = default(0.5);  // times long-term-sigma²
        // long-term window gets truncated to this length if threshold has been exceeded 
        int ratePredictionTruncatedLongWindowSteps = default(ratePredictionShortWindowSteps * 3);
        
        int qocRandomizationSteps = default(int(dropUnit(qocRandomizationInterval) * dropUnit(tickerFrequency)));
        double qocRandomizationSpread = default(0.02);
        int rateRandomizationSteps = default(int(dropUnit(rateRandomizationInterval) * dropUnit(tickerFrequency)));
        double rateRandomizationSpread = default(0.05);
        double rateJitter = default(0.15);
        
        int sensorPayload @unit(Byte) = default(300B);
        int controllerPayload @unit(Byte) = default(350B); 
}
