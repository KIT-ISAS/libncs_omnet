package libncs_omnet.util;

import inet.applications.contract.IFCPApp;

simple FCPTransport like IFCPApp
{
    parameters:
        int function = default(0);
        double percentage = default(0.1);
        bool usePreload = default(false);
        
        double collectionInterval @unit(s) = default(0.05s);
        double forcedPushDelay @unit(s) = default(0.04s);
        double fcpEpsilon = default(0.01);
        double fcpQ = default(0.1);
        
        bool enableQMSmoothing = default(false);
        int qmHistoryLength = default(10);
        
        // Overhead per packet induced by lower layers
        int lowerLayerOverhead @unit(B) = default(78Byte); // 7 Byte PPP + 40 Byte IPv6 + 8 Byte UDP
        // Overhead per packet for metadata attached to them
        int metadataOverhead @unit(B) = default(0Byte);
        
        //Overhead of fcp
        int fcpOverhead @unit(B) = default(71Byte);

    gates:
        // gate for incoming FCP packets
        input fcpIn @labels(FCPCommand/up);
        // gate for outgoing FCP packets
        output fcpOut @labels(FCPCommand/down);
        // gate for incoming and outgoing data/transport commands
        inout up @labels(TransportCommand);
}