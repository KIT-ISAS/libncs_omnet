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

#ifndef __LIBNCS_OMNET_TCP_NSC_SYSCTL_H_
#define __LIBNCS_OMNET_TCP_NSC_SYSCTL_H_

#ifndef WITH_TCP_NSC_SYSCTL
#error Please install NSC/TCP_NSC or disable 'TCP_NSC_SYSCTL' feature
#endif // ifndef WITH_TCP_NSC_SYSCTL

#include <omnetpp.h>

#include "inet/common/INETDefs.h"

#include <inet/transportlayer/tcp_nsc/TCP_NSC.h>

using namespace omnetpp;
using namespace inet;

class TCP_NSC_Sysctl : public tcp::TCP_NSC {

  protected:
    virtual void initialize(int stage) override;

};

#endif
