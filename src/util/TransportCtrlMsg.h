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

#ifndef UTIL_TRANSPORTCTRLMSG_H_
#define UTIL_TRANSPORTCTRLMSG_H_

#include "TransportCtrlMsg_m.h"

class TransportDataInfo: public TransportDataInfo_Base {
private:
    void copy(const TransportDataInfo& other);

public:
    TransportDataInfo(const char *name = nullptr, int kind = 0);
    TransportDataInfo(const TransportDataInfo& other);
    ~TransportDataInfo();

    TransportDataInfo& operator=(const TransportDataInfo& other);
    virtual TransportDataInfo *dup() const;

    virtual const inet::NetworkOptionsPtr& getNetworkOptions() const;
    virtual void setNetworkOptions(const inet::NetworkOptionsPtr& networkOptions);
    virtual inet::NetworkOptionsPtr replaceNetworkOptions(const inet::NetworkOptionsPtr networkOptions = nullptr);
};

#endif /* UTIL_TRANSPORTCTRLMSG_H_ */
