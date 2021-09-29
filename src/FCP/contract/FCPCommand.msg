cplusplus {{
#include <inet/networklayer/common/L3Address.h>
#include "CoCC/CoCCTranslator.h"

typedef inet::L3Address L3Address_t;
}}

class noncobject L3Address_t;
class noncobject ICoCCTranslatorPtr;

enum FcpCommandCode
{
    FCP_C_OPEN_ACTIVE = 1;   // active open
    FCP_C_OPEN_PASSIVE = 2;  // passive open a.k.a listen
    FCP_C_SEND = 3;          // send data
    FCP_C_TRANSLATOR = 4;	// set the translator
    FCP_C_CLOSE = 5;         // close connection
    FCP_C_STREAMSTART = 6;
}

enum FcpStatusIndication
{
	FCP_I_ESTABLISHED = 1;
	FCP_I_DATA = 2;
}

class FCPCommand
{
    int connId = -1;   // identifies the socket within the application
}

class FCPOpenCommand extends FCPCommand
{
    L3Address_t localAddr;
    L3Address_t remoteAddr;
    int localPort = -1;
    int remotePort = -1;
    int function;
    double percentage;
    bool usePreload;
    simtime_t collectionInterval;
    simtime_t forcedPushDelay;
    double fcpEpsilon;
    double fcpQ;
    
    bool enableQMSmoothing;
    int qmHistoryLength;
    
    int metadataOverhead;
    int lowerLayerOverhead;
    int fcpOverhead;
}

class FCPSendCommand extends FCPCommand
{
}

class FCPConnectionInfo extends FCPCommand
{
    L3Address_t localAddr;
    L3Address_t remoteAddr;
    int localPort;
    int remotePort;
}

class FCPTranslatorCommand extends FCPCommand
{
    ICoCCTranslatorPtr translator;
}

class FCPTransportStreamStartCommand extends FCPCommand
{
    simtime_t start;
}