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

#include "TCP_NSC_Sysctl.h"

#include <inet/common/InitStages.h>

Define_Module(TCP_NSC_Sysctl);


void TCP_NSC_Sysctl::initialize(int stage) {
    TCP_NSC::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        const char * const sysctlArgs = this->par("sysctl").stringValue();
        const std::vector<std::string> argsList = cStringTokenizer(sysctlArgs, ";").asVector();

        for (auto it = argsList.begin(); it != argsList.end(); it++) {
            const std::vector<std::string> splitList = cStringTokenizer(it->c_str(), "=").asVector();

            if (splitList.size() <= 1) {
                EV_WARN << "TCP_NSC_Sysctl ignoring sysctl line: " << *it << endl;

                continue;
            }

            pStackM->sysctl_set(splitList[0].c_str(), splitList[1].c_str());
        }

        for (size_t i = 0;; i++) {
            char sysctlName[256];
            char sysctlValue[256];

            const ptrdiff_t nameSize = pStackM->sysctl_getnum(i, sysctlName, sizeof(sysctlName));

            if (nameSize < 0) {
                break;
            }

            const ptrdiff_t valueSize = pStackM->sysctl_get(sysctlName, sysctlValue, sizeof(sysctlValue));

            if (valueSize < 0) {
                continue;
            }

            EV << "TCP_NSC: " << sysctlName << " = " << sysctlValue << endl;
        }
    }
}
