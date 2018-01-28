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

#ifndef __LIBNCS_OMNET_COCPNNCSCONTEXT_H_
#define __LIBNCS_OMNET_COCPNNCSCONTEXT_H_

#include <omnetpp.h>
#include <NcsContext.h>
#include <libncs_matlab.h>

using namespace omnetpp;

class CoCpnNcsContext : public NcsContext
{
  protected:
    virtual std::vector<const char *> getConfigFieldNames() override;
    virtual void setConfigValues(mwArray &cfgStruct) override;

  private:
    static const char CTRL_SEQ_LEN[];
    static const char MAX_CTRL_SEQ_DELAY[];
    static const char MAX_MEAS_DELAY[];
    static const char SAMPL_INTERVAL[];
    static const char SC_DELAY_PROBS[];
    static const char CA_DELAY_PROBS[];
};

#endif
