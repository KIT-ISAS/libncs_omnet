#ifndef __LIBNCS_OMNET_FCPPACKET_H
#define __LIBNCS_OMNET_FCPPACKET_H

#include <inet/transportlayer/contract/ITransportPacket.h>
#include "FCPPacket_m.h"

class FCPPacket : public FCPPacket_Base, public ITransportPacket
{
  private:
    void copy(const FCPPacket& other) {}

  public:
    FCPPacket(const char *name=NULL, int kind=0) : FCPPacket_Base(name,kind) {}
    FCPPacket(const FCPPacket& other) : FCPPacket_Base(other) {copy(other);}
    FCPPacket& operator=(const FCPPacket& other) {if (this==&other) return *this; FCPPacket_Base::operator=(other); copy(other); return *this;}

    virtual FCPPacket *dup() const override { return new FCPPacket(*this); }


    virtual unsigned int getSourcePort() const override { return FCPPacket_Base::getSrcPort(); }
    virtual void setSourcePort(unsigned int port) override { FCPPacket_Base::setSrcPort(port); }
    virtual unsigned int getDestinationPort() const override { return FCPPacket_Base::getDestPort(); }
    virtual void setDestinationPort(unsigned int port) override { FCPPacket_Base::setDestPort(port); }
};

#endif
