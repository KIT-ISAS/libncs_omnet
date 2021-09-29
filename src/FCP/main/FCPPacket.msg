cplusplus {{
#include "inet/common/INETDefs.h"
}}


//
// Represents an UDP packet, to be used with the ~UDP module.
//
packet FCPPacket
{
    @customize(true);
    @fieldNameSuffix("_var"); //TODO remove when OMNeT++ 4.x compatibility is no longer required

    unsigned short srcPort;	// 2B
    unsigned short destPort;// 2B
    double rtt;				// 0B, not necessary in header 
    double price;			// 0B, not necessary in header
    double ackPrice;		// 0B, only relevant in ACK
    double preload;			// 0B, preload not used
    double balance;			// 0B, used for preload
    unsigned int sequenceNo;// 2B
    unsigned int ackNo;		// 0B, only relevant in ACK
    int size;				// 0B, not necessary in header
    bool ack;				// 1B for 3 bools
	bool syn;
	bool fin;
	
	byteLength = 7;
}