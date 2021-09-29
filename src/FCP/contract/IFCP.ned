package libncs_omnet.FCP.contract;

moduleinterface IFCP{
    gates:
        input appIn[] @labels(FCPCommand/down);
        input ipIn @labels(FCPPacket,IPv4ControlInfo/up,IPv6ControlInfo/up);
        output appOut[] @labels(FCPCommand/up);
        output ipOut @labels(FCPPacket,IPv4ControlInfo/down,IPv6ControlInfo/down);
}