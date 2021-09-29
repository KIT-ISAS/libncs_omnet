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

#include "MatlabContext.h"

#include <libncs_matlab.h>
#include <MatlabScheduler.h>

long MatlabContext::MatlabToken::nextTokenId = 0;
bool MatlabContext::matlabInitialized = false;
std::set<long> MatlabContext::activeTokens;


MatlabContext::MatlabToken::MatlabToken() : id(-1) {
    // token is per default initialized with invalid value
}

MatlabContext::MatlabToken::MatlabToken(const bool generate) : id(nextTokenId++) {
}

bool MatlabContext::MatlabToken::isValid() const {
    return id >= 0;
}

MatlabContext::MatlabToken MatlabContext::requestToken() {
    const MatlabToken token(true);

    activeTokens.insert(token.id);

    initializeMatlab();

    return token;
}

void MatlabContext::returnToken(const MatlabToken token) {
    if (!token.isValid()) {
        return;
    }

    activeTokens.erase(token.id);

    if (activeTokens.empty()) {
        deinitializeMatlab();
    }
}

void MatlabContext::initializeMatlab() {
    EV_STATICCONTEXT;

    auto sched = dynamic_cast<MatlabScheduler*>(getSimulation()->getScheduler());

    if (!sched) {
        throw cRuntimeError("MatlabScheduler not loaded, insert 'scheduler-class = MatlabScheduler' into the section [General] of your configuration");
    } else {
        sched->startMatlabRuntime();
    }

    if (matlabInitialized) {
        return; // nothing to do
    }

    // Load the required MATLAB code into the MATLAB Runtime.
    if (!libncs_matlabInitialize()) {
        const char * const err = mclGetLastErrorMessage();

        EV_FATAL << "could not initialize the library properly: "
                        << err << endl;
        EV << "could not initialize the library properly: "
                << err << endl;

        throw cRuntimeError("initializeLib failed");
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

    matlabInitialized = true;

    EV << "NcsManager initialized" << endl;
}

void MatlabContext::deinitializeMatlab() {
    EV_STATICCONTEXT;

    if (!matlabInitialized) {
        return; // nothing to do
    }

    // Release the resources used by the generated MATLAB code
    libncs_matlabTerminate();

    matlabInitialized = false;

    EV << "NcsManager deinitialized" << endl;
}
