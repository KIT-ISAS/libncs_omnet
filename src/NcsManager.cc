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

#include <libncs_matlab.h>
#include <NcsManager.h>

#include <iostream>

Define_Module(NcsManager);

NcsManager::NcsManager() {
    // Load the required MATLAB code into the MATLAB Runtime.
    if (!libncs_matlabInitialize()) {
        const  char * const err = mclGetLastErrorMessage();

        std::cerr << "could not initialize the library properly: "
                        << err << std::endl;
        EV << "could not initialize the library properly: "
                << err << std::endl;

        error("initializeLib failed");
    }

    const char * const seedSet = getEnvir()->getConfig()->getConfigValue("seed-set");

    if (seedSet != nullptr) {
        const long seed = cConfiguration::parseLong(seedSet, nullptr);

        EV << "Seeding MATLAB RNG with value " << seed << endl;

        const mwArray mwSeed = mwArray(seed);

        ncs_seedRng(mwSeed);
    } else {
        EV << "WARNING: NOT SEEDING MATLAB RNG" << endl;
    }

    EV << "NcsManager initialized" << std::endl;
}

NcsManager::~NcsManager() {
    // Release the resources used by the generated MATLAB code
    libncs_matlabTerminate();

    EV << "NcsManager deinitialized" << std::endl;
}

int NcsManager::numInitStages() const {
    return 0;
}

void NcsManager::initialize(const int stage) {
    // nothing to do
}

void NcsManager::handleMessage(cMessage * const msg) {
    error("NcsManager received unexpected message");
}
