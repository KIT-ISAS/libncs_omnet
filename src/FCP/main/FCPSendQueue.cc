#include "FCPSendQueue.h"

/**
 * Basic destructor, which empties the queue.
 */
FCPSendQueue::~FCPSendQueue(){
    for (auto & elem : queue)
        delete elem;
}

/**
 * Adds a new cPacket to the queue.
 */
void FCPSendQueue::enqueueData(cPacket *payload){
    queue.push_back(payload);
}

/*
 * Creates a new fcp packet with the cPacket at the front of the queue.
 */
FCPPacket *FCPSendQueue::createPacket(){
    if(!isEmpty()){
        cPacket *payload = queue.front();
        queue.pop_front();
        FCPPacket *packet = new FCPPacket(payload->getName());
        packet->encapsulate(payload);
        return packet;
    }
    else{
        return nullptr;
    }
}
