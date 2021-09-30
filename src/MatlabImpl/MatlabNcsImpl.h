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

#ifndef MATLABIMPL_MATLABNCSIMPL_H_
#define MATLABIMPL_MATLABNCSIMPL_H_

#include <NcsContext.h>
#include "MatlabContext.h"

#include <libncs_matlab.h>

#include <omnetpp.h>

using namespace omnetpp;


class MatlabInitializer {
  protected:
    MatlabInitializer();
    virtual ~MatlabInitializer();

    MatlabContext::MatlabToken matlabToken;
};

class MatlabNcsImpl: virtual public AbstractNcsImpl, public MatlabInitializer, public cSimpleModule {
  public:

    MatlabNcsImpl();
    virtual ~MatlabNcsImpl();

    virtual void initializeNcs(NcsContext * const context) override;
    virtual void finishNcs() override;

    virtual const simtime_t& getPlantPeriod() override;
    virtual const simtime_t& getControlPeriod() override;

    virtual void doPlantStep(const simtime_t& ncstTime, NcsContext::NcsPlantStepResult * const result) override;
    virtual void doControlStep(const simtime_t& ncstTime, NcsContext::NcsControlStepResult * const result) override;
    virtual std::vector<NcsContext::NcsPkt> handlePacket(const simtime_t& ncsTime, NcsContext::NcsPkt& ncsPkt) override;

  protected:

    struct RcvdPktEvt {
        simtime_t sent;
        simsignal_t signal;
    };

    NcsContext * context;
    const NcsContext::NcsParameters * parameters;
    const NcsContext::NcsSignals * signals;
    std::vector<RcvdPktEvt> pktEvts;

    /**
     * The interval between two consecutive plant computation steps;
     */
    simtime_t plantPeriod;
    /**
    * The interval between two consecutive control computation steps.
    */
    simtime_t controlPeriod;
    /**
     * Stores the ComponentMap key of the corresponding NetworkedControlSystem instance in Matlab.
     */
    mwArray ncsHandle;
    /**
     * Last QoC reported by controller
     * //TODO move to CoCPN-specific code?
     */
    double reportedQoC;

  protected:

    void updateControlPeriod(const simtime_t newControlPeriod);

    void parseMatlabPktList(const mwArray& mw_ncsPktList, std::vector<NcsContext::NcsPkt> & ncsPkts);
    void parseMatlabPkt(const mwArray& mw_pkt, NcsContext::NcsPkt& ncsPkt);
    uint64_t getMatlabPktId(const mwArray& mw_pkt);
    bool matlabPktIsAck(const mwArray& mw_pkt);
    mwArray ncsPktToMatlabPkt(NcsContext::NcsPkt& pkt);

    void recordPlantStatistics(mwArray& plantStatistics);
    void recordControllerStatistics(mwArray& controllerStatistics);
    const std::vector<cOutVector*> createNumericStatisticsOutVectors(const std::string& statName, uint32_t numComponents);

    mwArray createNcsConfigStruct();
    virtual std::vector<const char *> getConfigFieldNames();
    virtual void setConfigValues(mwArray &cfgStruct);
    void testNonnegBool(std::vector<const char *> &fieldNames, cPar * const par);
    void testNonnegLong(std::vector<const char *> &fieldNames, cPar * const par);
    void testPositiveLong(std::vector<const char *> &fieldNames, cPar * const par);
    void testNonnegDbl(std::vector<const char *> &fieldNames, cPar * const par);
    void testNonemptyDblVect(std::vector<const char *> &fieldNames, cPar * const par);
    void testNonemptyString(std::vector<const char *> &fieldNames, cPar * const par);
    void setNonnegBool(mwArray &cfgStruct, cPar * const par);
    void setNonnegLong(mwArray &cfgStruct, cPar * const par);
    void setPositiveLong(mwArray &cfgStruct, cPar * const par);
    void setNonnegDbl(mwArray &cfgStruct, cPar * const par);
    void setNonemptyDblVect(mwArray &cfgStruct, const char * name, const std::vector<double> &vect);
    void setNonemptyDblVect(mwArray &cfgStruct, cPar * const par);
    void setNonemptyString(mwArray &cfgStruct, cPar * const par);

  protected:

    virtual void initialize();
    virtual void handleMessage(cMessage * const msg);
};

#endif /* MATLABIMPL_MATLABNCSIMPL_H_ */
