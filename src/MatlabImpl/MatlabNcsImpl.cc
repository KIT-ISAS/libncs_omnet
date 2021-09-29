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

#include "MatlabNcsImpl.h"

Define_Module(MatlabNcsImpl);


MatlabInitializer::MatlabInitializer() {
    matlabToken = MatlabContext::requestToken();
}

MatlabInitializer::~MatlabInitializer() {
    MatlabContext::returnToken(matlabToken);
}


MatlabNcsImpl::MatlabNcsImpl() {
    // nothing to do ... yet
}

MatlabNcsImpl::~MatlabNcsImpl() {
    // nothing to do ... yet
}

void MatlabNcsImpl::initializeNcs(NcsContext * const context) {
    ASSERT(context);

    this->context = context;
    parameters = context->getParameters();
    signals = context->getSignals();

    const mwArray mw_ncsId(parameters->ncsId);
    const mwArray mw_configFile(parameters->configFile->stringValue());
    const mwArray mw_maxSimTime(std::max((int64_t) 0, (parameters->simTimeLimit - parameters->startupDelay).inUnit(SIMTIME_PS)));

    const mwArray mw_configStruct = createNcsConfigStruct();

    ncs_initialize(1, ncsHandle, mw_maxSimTime, mw_ncsId, mw_configStruct, mw_configFile);

    // get period durations for plant and controller
    mwArray mw_controlPeriod;
    mwArray mw_plantPeriod;

    ncs_getTickerInterval(1, mw_controlPeriod, ncsHandle);
    ncs_getPlantTickerInterval(1, mw_plantPeriod, ncsHandle);

    // should both be double values, but integers are required by OMNeT++
    // we strip off the fractional part, the MATLAB part anticipates this behavior
    ASSERT(mw_controlPeriod.ClassID() == mxDOUBLE_CLASS);
    ASSERT(mw_plantPeriod.ClassID() == mxDOUBLE_CLASS);

    controlPeriod = SimTime(static_cast<uint64_t>(mw_controlPeriod), SIMTIME_PS);
    plantPeriod = SimTime(static_cast<uint64_t>(mw_plantPeriod), SIMTIME_PS);
}

void MatlabNcsImpl::finishNcs() {
    // record NCS statistics
    try {
        mwArray mw_costs;
        mwArray mw_ncsStats;
        mwArray mw_plantStats;

        ncs_finalize(3, mw_costs, mw_ncsStats, mw_plantStats, ncsHandle);

        ASSERT(mw_plantStats.NumberOfElements() == 1);
        ASSERT(mw_plantStats.ClassID() == mxSTRUCT_CLASS);
        ASSERT(mw_ncsStats.NumberOfElements() == 1);
        ASSERT(mw_ncsStats.ClassID() == mxSTRUCT_CLASS);

        context->recordScalar("total_control_costs", static_cast<double>(mw_costs));
        recordPlantStatistics(mw_plantStats);
        recordControllerStatistics(mw_ncsStats);

    } catch (const mwException& e) {
            EV_ERROR << "caught mwException in MatlabNcsContext::finish for NCS with id " << parameters->ncsId << endl;
            EV_ERROR << e.what() << std::endl;
            const_cast<mwException&>(e).print_stack_trace();

            throw e;
    }
}

const simtime_t& MatlabNcsImpl::getPlantPeriod() {
    return plantPeriod;
}

const simtime_t& MatlabNcsImpl::getControlPeriod() {
    return controlPeriod;
}

void MatlabNcsImpl::doPlantStep(const simtime_t& ncsTime, NcsContext::NcsPlantStepResult * const result) {
    ASSERT(result);

    int64_t currentSimtime = ncsTime.inUnit(SIMTIME_PS);

    mwArray mw_plantStateAdmissible;
    mwArray mw_timestamp(currentSimtime);

    ncs_doPlantStep(1, mw_plantStateAdmissible, ncsHandle, mw_timestamp);

    result->plantStateAdmissible = static_cast<bool>(mw_plantStateAdmissible);
}

void MatlabNcsImpl::doControlStep(const simtime_t& ncsTime, NcsContext::NcsControlStepResult * const result) {
    ASSERT(result);

    int64_t currentSimtime = ncsTime.inUnit(SIMTIME_PS);

//  mwArray mw_paramStruct(mxSTRUCT_CLASS);
//  const char* fields[] = {"controllerDeadband"};
//  mwArray mw_paramStruct(1, 1, 1, fields);
//  const mwArray value(42);
//  mw_paramStruct("controllerDeadband", 1, 1).Set(value);
    // for now: re-use params of simulation configuration
    // may be functions evaluated by omnet to vary parameters during
    // simulation run-time
    mwArray mw_paramStruct = createNcsConfigStruct();
    mwArray mw_ncsPktList;
    mwArray mw_reportedQoc;
    mwArray mw_ncsStats;
    mwArray mw_controllerStateAdmissible;
    mwArray mw_timestamp(currentSimtime);

    ncs_doLoopStep(4, mw_ncsPktList, mw_reportedQoc, mw_ncsStats, mw_controllerStateAdmissible, ncsHandle, mw_timestamp, mw_paramStruct);

    reportedQoC = mw_reportedQoc;
    result->controllerStateAdmissible = static_cast<bool>(mw_controllerStateAdmissible);

    context->emit(signals->controlErrorSignal, static_cast<double>(mw_ncsStats("actual_control_error", 1, 1)));
    // for some reason, the additional call of Get(1,1) is required to avoid zeros
    context->emit(signals->estControlErrorSignal, static_cast<double>(mw_ncsStats("estimated_control_error", 1, 1).Get(1,1)));
    context->emit(signals->stageCostsSignal, static_cast<double>(mw_ncsStats("actual_stagecosts", 1, 1).Get(1,1)));

    mwArray mw_scDelays = mw_ncsStats("sc_delays", 1, 1);
    mwArray mw_caDelays = mw_ncsStats("ca_delays", 1, 1);
    mwArray mw_acDelays = mw_ncsStats("ac_delays", 1, 1);
    const size_t scCount = mw_scDelays.NumberOfElements();
    const size_t caCount = mw_caDelays.NumberOfElements();
    const size_t acCount = mw_acDelays.NumberOfElements();

    for (size_t i = 1; i <= scCount; i++) {
        context->emit(signals->scObservedDelaySignal, static_cast<uint64_t>(mw_scDelays(i)) * controlPeriod);
    }
    for (size_t i = 1; i <= caCount; i++) {
        context->emit(signals->caObservedDelaySignal, static_cast<uint64_t>(mw_caDelays(i)) * controlPeriod);
    }
    for (size_t i = 1; i <= acCount; i++) {
        context->emit(signals->acObservedDelaySignal, static_cast<uint64_t>(mw_acDelays(i)) * controlPeriod);
    }

    parseMatlabPktList(mw_ncsPktList, result->pkts);
}

std::vector<NcsContext::NcsPkt> MatlabNcsImpl::handlePacket(const simtime_t& ncsTime, NcsContext::NcsPkt& ncsPkt) {
    mwArray mw_ncsPktList;
    mwArray mw_timestamp(ncsTime.inUnit(SIMTIME_PS));
    mwArray mw_pkt = ncsPktToMatlabPkt(ncsPkt);

    ncs_doHandlePacket(1, mw_ncsPktList, ncsHandle, mw_timestamp, mw_pkt);

    std::vector<NcsContext::NcsPkt> result;

    parseMatlabPktList(mw_ncsPktList, result);

    return result;
}

void MatlabNcsImpl::updateControlPeriod(const simtime_t newControlPeriod) {
    if (newControlPeriod != controlPeriod) {
        controlPeriod = newControlPeriod;
        context->updateControlPeriod(newControlPeriod);
    }
}

void MatlabNcsImpl::parseMatlabPktList(const mwArray& mw_ncsPktList, std::vector<NcsContext::NcsPkt> & ncsPkts) {
    // transform obtained byte arrays to NcsPkts

    size_t cpsPktCount = mw_ncsPktList.NumberOfElements();

    ncsPkts.resize(cpsPktCount);

    if (cpsPktCount != 0) {
        // mw_cpsPktList with packets to send to a certain node should be a
        // cpsPktCount-by-1 cell array (column-vector like)
        ASSERT(mw_ncsPktList.NumberOfDimensions() == 2);
        ASSERT((size_t ) mw_ncsPktList.GetDimensions()(1) == cpsPktCount);
        ASSERT((size_t ) mw_ncsPktList.GetDimensions()(2) == 1);
        ASSERT(mw_ncsPktList.ClassID() == mxCELL_CLASS);

        for (uint32_t pktNum = 0; pktNum < cpsPktCount; pktNum++) {
            parseMatlabPkt(mw_ncsPktList(pktNum + 1), ncsPkts[pktNum]);
        }
    }
}

void MatlabNcsImpl::parseMatlabPkt(const mwArray& mw_pkt, NcsContext::NcsPkt& ncsPkt) {
    // transform the MATLAB NCS packet into an OMNeT++/INET RawPacket, wrapped into an NcsPkt

    ASSERT(mw_pkt.NumberOfElements() == 1);

    mwArray mw_src, mw_dst, mw_payload;

    ncs_pktGetSrcAddr(1, mw_src, mw_pkt);
    ncs_pktGetDstAddr(1, mw_dst, mw_pkt);
    ncs_pktGetPayload(1, mw_payload, mw_pkt);

    // the payload should be a byte stream (row vector like 1-by-n)
    ASSERT(mw_payload.NumberOfDimensions() == 2);
    ASSERT((size_t ) mw_payload.GetDimensions()(1) == 1);
    ASSERT(mw_payload.ClassID() == mxUINT8_CLASS);

    RawPacket* const rawPkt = new RawPacket();
    ByteArray payloadArray;
    uint8_t payloadBuf[mw_payload.NumberOfElements()];

    mw_payload.GetData(payloadBuf, sizeof(payloadBuf));
    payloadArray.setDataFromBuffer(payloadBuf, sizeof(payloadBuf));

    rawPkt->setByteLength(sizeof(payloadBuf));
    rawPkt->setByteArray(payloadArray);

    const int srcIndex = mw_src;
    const int dstIndex = mw_dst;

    ASSERT(srcIndex < NCTXCI_COUNT);
    ASSERT(dstIndex < NCTXCI_COUNT);

    ncsPkt.src = static_cast<NcsContextComponentIndex>(srcIndex);
    ncsPkt.dst = static_cast<NcsContextComponentIndex>(dstIndex);
    ncsPkt.pktId = getMatlabPktId(mw_pkt);
    ncsPkt.isAck = matlabPktIsAck(mw_pkt);
    ncsPkt.pkt = rawPkt;
}

uint64_t MatlabNcsImpl::getMatlabPktId(const mwArray& mw_pkt) {
    mwArray mw_id;

    ncs_pktGetId(1, mw_id, mw_pkt);

    return static_cast<uint64_t>(mw_id);
}

bool MatlabNcsImpl::matlabPktIsAck(const mwArray& mw_pkt) {
    mwArray mw_isAck;

    ncs_pktIsAck(1, mw_isAck, mw_pkt);

    return static_cast<bool>(mw_isAck);
}

mwArray MatlabNcsImpl::ncsPktToMatlabPkt(NcsContext::NcsPkt& pkt) {
    // transform the NcsPkt / OMNeT++/INET RawPacket to a MATLAB NCS packet

    const size_t payloadSize = pkt.pkt->getByteArray().getDataArraySize();

    mwArray mw_payload(1, &payloadSize, mxUINT8_CLASS);

    mw_payload.SetData(reinterpret_cast<mxUint8 *>(pkt.pkt->getByteArray().getDataPtr()), payloadSize);

    EV_DEBUG << "Attempt to create Matlab DataPacket from raw packet" << endl;

    mwArray mw_src(pkt.src);
    mwArray mw_dst(pkt.dst);
    mwArray mw_pkt;

    ncs_pktCreate(1, mw_pkt, mw_src, mw_dst, mw_payload);

    pkt.pktId = getMatlabPktId(mw_pkt);

    return mw_pkt;
}

void MatlabNcsImpl::recordPlantStatistics(mwArray& plantStatistics) {

    for (int i = 0; i < plantStatistics.NumberOfFields(); ++i) {
        const mwString f = plantStatistics.GetFieldName(i);
        const std::string statName(static_cast<const char*>(f));
        const mwArray currStat = plantStatistics(statName.c_str(), 1, 1);

        const mwArray dims = currStat.GetDimensions();
        const uint32_t numRows = dims(1);

        auto statsVecs = this->createNumericStatisticsOutVectors(statName, numRows);

        const uint32_t numElements = dims(2);
        for (uint32_t i = 0; i < numElements; ++i) {
            const simtime_t time = this->plantPeriod * i + parameters->startupDelay;

            for (uint32_t j = 0; j < numRows; ++j) {
                statsVecs[j]->recordWithTimestamp(time, (double) currStat(j + 1, i + 1));
            }
        }
        for (uint32_t i = 0; i < numRows; ++i) {
            delete statsVecs[i];
        }
        statsVecs.clear();
    }
}

void MatlabNcsImpl::recordControllerStatistics(mwArray& controllerStatistics) {
    std::string times("controllerTimes");

    mwArray controllerTimes = controllerStatistics(times.c_str(), 1, 1);
    uint32_t numTimes = controllerTimes.NumberOfElements();
    const uint32_t timesDim = controllerTimes.GetDimensions()(1);
    ASSERT(controllerTimes.NumberOfDimensions() == 2);
    ASSERT(timesDim == 1); // row vector
    ASSERT(controllerTimes.IsNumeric());

    for (int i = 0; i < controllerStatistics.NumberOfFields(); ++i) {
        const mwString f = controllerStatistics.GetFieldName(i);
        const std::string statName(static_cast<const char*>(f));
        if (statName.compare(times) != 0) {
            const mwArray currStat = controllerStatistics(statName.c_str(), 1, 1);

            const mwArray dims = currStat.GetDimensions();
            const uint32_t numRows = dims(1);
            const uint32_t numElements = dims(2);
            auto statsVecs = this->createNumericStatisticsOutVectors(statName, numRows);

            // skip the first time index if number of elements is less than number of times
            // indices start at 1 when accessing matlab arrays
            uint32_t timeIdx = (numElements == numTimes-1) ? 2 : 1;
            for (uint32_t i = 0; i < numElements; ++i) {
                const simtime_t time = (double) controllerTimes(timeIdx++) + parameters->startupDelay;

                for (uint32_t j = 0; j < numRows; ++j) {
                    statsVecs[j]->recordWithTimestamp(time, (double) currStat(j + 1, i + 1));
                }
            }
            for (uint32_t i = 0; i < numRows; ++i) {
                delete statsVecs[i];
            }
            statsVecs.clear();
        }

    }
}

const std::vector<cOutVector*> MatlabNcsImpl::createNumericStatisticsOutVectors(const std::string& statName, uint32_t numComponents) {

    if (numComponents == 1) {
        auto vec = new cOutVector(statName.c_str());
        vec->setType(cOutVector::TYPE_DOUBLE);

        return std::vector<cOutVector*> ({vec});
    }

    auto result = std::vector<cOutVector*> ();
    for (uint32_t i = 0; i< numComponents; ++i) {
        auto vec = new cOutVector((statName + "(" + std::to_string(i+1) + ")").c_str());
        vec->setType(cOutVector::TYPE_DOUBLE);
        result.push_back(vec);
    }
    return result;
}

mwArray MatlabNcsImpl::createNcsConfigStruct() {
    const std::vector<const char *> fields = getConfigFieldNames();

    mwArray result = fields.size() > 0 ? mwArray(1, 1, fields.size(), const_cast<const char **>(fields.data())) : mwArray(mxSTRUCT_CLASS);

    setConfigValues(result);

    return result;
}

std::vector<const char *> MatlabNcsImpl::getConfigFieldNames() {
    auto result = std::vector<const char *>();

    testNonemptyString(result, parameters->controllerClassName);
    testNonemptyString(result, parameters->filterClassName);
    testPositiveLong(result, parameters->networkType);
    testPositiveLong(result, parameters->controlSequenceLength);
    testNonnegLong(result, parameters->maxMeasDelay);
    testPositiveLong(result, parameters->mpcHorizon);
    testNonnegDbl(result, parameters->controlErrorWindowSize);

    return result;
}

void MatlabNcsImpl::setConfigValues(mwArray &cfgStruct) {
    setNonemptyString(cfgStruct, parameters->controllerClassName);
    setNonemptyString(cfgStruct, parameters->filterClassName);
    setPositiveLong(cfgStruct, parameters->networkType);
    setPositiveLong(cfgStruct, parameters->controlSequenceLength);
    setNonnegLong(cfgStruct, parameters->maxMeasDelay);
    setPositiveLong(cfgStruct, parameters->mpcHorizon);
    setNonnegDbl(cfgStruct, parameters->controlErrorWindowSize);
}

void MatlabNcsImpl::testNonnegBool(std::vector<const char *> &fieldNames, cPar * const par) {
    testNonnegLong(fieldNames, par);
}

void MatlabNcsImpl::testNonnegLong(std::vector<const char *> &fieldNames, cPar * const par) {
    if (par->intValue() >= 0) {
        fieldNames.push_back(par->getName());
    }
}

void MatlabNcsImpl::testPositiveLong(std::vector<const char *> &fieldNames, cPar * const par) {
    if (par->intValue() > 0) {
        fieldNames.push_back(par->getName());
    }
}

void MatlabNcsImpl::testNonnegDbl(std::vector<const char *> &fieldNames, cPar * const par) {
    if (par->doubleValue() >= 0) {
        fieldNames.push_back(par->getName());
    }
}

void MatlabNcsImpl::testNonemptyDblVect(std::vector<const char *> &fieldNames, cPar * const par) {
    if (par->stdstringValue().length() > 0) {
        cStringTokenizer tokens(par->stringValue());
        const std::vector<double> dbls = tokens.asDoubleVector();
        const size_t values = dbls.size();

        if (values > 0) {
            fieldNames.push_back(par->getName());
        }
    }
}

void MatlabNcsImpl::testNonemptyString(std::vector<const char *> &fieldNames, cPar * const par) {
    if (par->stdstringValue().length() > 0) {
        fieldNames.push_back(par->getName());
    }
}

void MatlabNcsImpl::setNonnegBool(mwArray &cfgStruct, cPar * const par) {
    if (par->intValue() >= 0) {
        const mwArray parValue(par->intValue() > 0);

        cfgStruct(par->getName(), 1, 1).Set(parValue);
    }
}

void MatlabNcsImpl::setNonnegLong(mwArray &cfgStruct, cPar * const par) {
    if (par->intValue() >= 0) {
        const mwArray parValue(par->intValue());

        cfgStruct(par->getName(), 1, 1).Set(parValue);
    }
}

void MatlabNcsImpl::setPositiveLong(mwArray &cfgStruct, cPar * const par) {
    if (par->intValue() > 0) {
        const mwArray parValue(par->intValue());

        cfgStruct(par->getName(), 1, 1).Set(parValue);
    }
}

void MatlabNcsImpl::setNonnegDbl(mwArray &cfgStruct, cPar * const par) {
    if (par->doubleValue() >= 0) {
        const mwArray parValue(par->doubleValue());

        cfgStruct(par->getName(), 1, 1).Set(parValue);
    }
}

void MatlabNcsImpl::setNonemptyDblVect(mwArray &cfgStruct, const char * name, const std::vector<double> &vect) {
    const size_t values = vect.size();

    mwArray mwValues(1, &values, mxDOUBLE_CLASS);

    mwValues.SetData(const_cast<double *>(vect.data()), values);

    cfgStruct(name, 1, 1).Set(mwValues);
}

void MatlabNcsImpl::setNonemptyDblVect(mwArray &cfgStruct, cPar * const par) {
    if (par->stdstringValue().length() > 0) {
        cStringTokenizer tokens(par->stringValue());
        const std::vector<double> dbls = tokens.asDoubleVector();
        const size_t values = dbls.size();

        if (values > 0) {
            setNonemptyDblVect(cfgStruct, par->getName(), dbls);
        }
    }
}

void MatlabNcsImpl::setNonemptyString(mwArray &cfgStruct, cPar * const par) {
    if (par->stdstringValue().length() > 0) {
        const mwArray parValue(par->stringValue());

        cfgStruct(par->getName(), 1, 1).Set(parValue);
    }
}

void MatlabNcsImpl::initialize() {
    // nothing to do
}

void MatlabNcsImpl::handleMessage(cMessage * const msg) {
    error("MatlabNcsImpl received unexpected message");
}
