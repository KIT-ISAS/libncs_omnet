#ifndef __LIBNCS_OMNET_FCPSENDQUEUE_H
#define __LIBNCS_OMNET_FCPSENDQUEUE_H

#include <list>

#include "FCPConnection.h"
#include "FCPPacket.h"

class FCPSendQueue : public cObject
{
    protected:
        FCPConnection *conn;

        typedef std::list<cPacket *> PayloadQueue;
        PayloadQueue queue;

    public:
        FCPSendQueue(){conn = nullptr;}

        ~FCPSendQueue();

        void setConnection(FCPConnection *connection){conn = connection;}

        void enqueueData(cPacket *payload);

        FCPPacket *createPacket();

        bool isEmpty(){return queue.empty();}
};
#endif
