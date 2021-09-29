package libncs_omnet.FCP.main;

import libncs_omnet.FCP.contract.IFCP;


simple FCP like IFCP
{
    parameters:
        
        @signal[rate](type="double");
        
        @statistic[rate](source=rate;title="FCP price";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[priceEndp](type="double");
        
        @statistic[priceEndp](source=priceEndp;title="FCP price";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[budget](type="double");
        
        @statistic[budget](source=budget;title="FCP price";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[targetBudget](type="double");
        
        @statistic[targetBudget](source=targetBudget;title="FCP target budget";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[averageQoc](type="double");
        
        @statistic[averageQoc](source=averageQoc;title="FCP averaged qoc value";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[avgBNQMToCalc](type="double");
        
        @statistic[avgBNQMToCalc](source=avgBNQMToCalc;title="FCP average bottleneck QM for calculation";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[avgBNQMToAnnounce](type="double");
        
        @statistic[avgBNQMToAnnounce](source=avgBNQMToAnnounce;title="FCP average bottleneck QM for announcing";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[avgBNBudget](type="double");
        
        @statistic[avgBNBudget](source=avgBNBudget;title="FCP average bottleneck budget";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[avgTargetQM](type="double");
        
        @statistic[avgTargetQM](source=avgTargetQM;title="FCP average target QM";record=stats,vector?;interpolationmode=sample-hold);
        
        
        
        @signal[numBNFlows](type="long");
        
        @statistic[numBNFlows](source=numBNFlows;title="Num BN flows";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[changedBudget](type="double");
        
        @statistic[changedBudget](source=changedBudget;title="Changed budget";record=stats,vector?;interpolationmode=sample-hold);
        
        @signal[qmDiff](type="double");
        
        @statistic[qmDiff](source=qmDiff;title="Changed budget";record=stats,vector?;interpolationmode=sample-hold);
        
        

    gates:
        input appIn[] @labels(FCPCommand/down);
        input ipIn @labels(FCPPacket,IPv4ControlInfo/up,IPv6ControlInfo/up);
        output appOut[] @labels(FCPCommand/up);
        output ipOut @labels(FCPPacket,IPv4ControlInfo/down,IPv6ControlInfo/down);
}