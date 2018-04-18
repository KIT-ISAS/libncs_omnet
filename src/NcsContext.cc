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

#include <inet/common/InitStages.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <NcsCpsApp.h>
#include "NcsContext.h"
#include "messages/NcsCtrlMsg_m.h"

Define_Module(NcsContext);

const char NcsContext::ACTUATOR_NAME[] = "actuator";
const char NcsContext::CONTROLLER_NAME[] = "controller";
const char NcsContext::SENSOR_NAME[] = "sensor";

const std::string NcsContext::ACTUATOR_GATE = "actuator";
const std::string NcsContext::CONTROLLER_GATE = "controller";
const std::string NcsContext::SENSOR_GATE = "sensor";
const std::string * NcsContext::GATE_NAMES[] = { &ACTUATOR_GATE, &CONTROLLER_GATE, &SENSOR_GATE };

int NcsContext::ncsIdCounter = 1;

int NcsContext::numInitStages() const {
    return INITSTAGE_LAST;
}

void NcsContext::initialize(const int stage) {
    switch (stage) {
    case INITSTAGE_LOCAL:
        // reset at the beginning of each simulation to get the same IDs for repeated runs
        ncsIdCounter = 1;

        // NcsManager is initialized in this stage
        configFile = par("configFile").stdstringValue();
        startupDelay = par("startupDelay").doubleValue();

        // setup signals for statistics recording
        scSentSignal = registerSignal("sc_sent");
        caSentSignal = registerSignal("ca_sent");
        acSentSignal = registerSignal("ac_sent");
        qocSignal = registerSignal("act_qoc");
        scObservedDelaySignal = registerSignal("sc_delay_obs");
        caObservedDelaySignal = registerSignal("ca_delay_obs");
        acObservedDelaySignal = registerSignal("ac_delay_obs");

        scActualDelaySignal = registerSignal("sc_delay_act");
        caActualDelaySignal = registerSignal("ca_delay_act");
        acActualDelaySignal = registerSignal("ac_delay_act");
        break;
    case INITSTAGE_LOCAL + 1: {
        ncsId = ncsIdCounter++; // draw an identifier

        // initialize NCS
        const mwArray mw_ncsId(ncsId);
        const mwArray mw_configFile(configFile.c_str());

        SimTime simTimeLimit = SimTime::parse(cSimulation::getActiveEnvir()->getConfig()->getConfigEntry("sim-time-limit").getValue());
        const mwArray mw_maxSimTime((simTimeLimit -  startupDelay).inUnit(SIMTIME_PS));

        mwArray mw_configStruct = this->createNcsConfigStruct();
        ncs_initialize(1, ncsHandle, mw_maxSimTime, mw_ncsId, mw_configStruct, mw_configFile);

        // setup periodic ticker event for NCS data processing
        mwArray mw_tickerInterval;

        ncs_getTickerInterval(1, mw_tickerInterval, ncsHandle);
        tickerInterval = SimTime(static_cast<uint64_t>(mw_tickerInterval), SIMTIME_PS);

        cMessage * const tickerMsg = new cMessage("NcsTickerEvent", NCTXMK_TICKER_EVT);

        scheduleAt(startupDelay + tickerInterval, tickerMsg);

        break; }
    case INITSTAGE_APPLICATION_LAYER: {
        // IP auto assignment, lookup address by name
        // self.prefix + "controller|actuator|sensor"

        cModule * const parent = getParentModule();

        ASSERT(parent);

        const std::string parentPath = parent->getFullPath();

        L3AddressResolver().tryResolve((parentPath + "." + ACTUATOR_NAME).c_str(), cpsAddr[NCTXCI_ACTUATOR]);
        L3AddressResolver().tryResolve((parentPath + "." + CONTROLLER_NAME).c_str(), cpsAddr[NCTXCI_CONTROLLER]);
        L3AddressResolver().tryResolve((parentPath + "." + SENSOR_NAME).c_str(), cpsAddr[NCTXCI_SENSOR]);

        if (cpsAddr[NCTXCI_ACTUATOR].isUnspecified()) {
            error(("Unable to resolve address of actuator for NCS " + parentPath).c_str());
        }
        if (cpsAddr[NCTXCI_CONTROLLER].isUnspecified()) {
            error(("Unable to resolve address of controller for NCS " + parentPath).c_str());
        }
        if (cpsAddr[NCTXCI_SENSOR].isUnspecified()) {
            error(("Unable to resolve address of sensor for NCS " + parentPath).c_str());
        }

        EV << parent->getFullPath() << "." << ACTUATOR_NAME << " is " << cpsAddr[NCTXCI_ACTUATOR].str() << endl;
        EV << parent->getFullPath() << "." << CONTROLLER_NAME << " is " << cpsAddr[NCTXCI_CONTROLLER].str() << endl;
        EV << parent->getFullPath() << "." << SENSOR_NAME << " is " << cpsAddr[NCTXCI_SENSOR].str() << endl;

        // connect Controller to Actuator and Sensor
        connect(NCTXCI_ACTUATOR);
        connect(NCTXCI_SENSOR);

        break; }
    }
}

void NcsContext::finish() {
    EV << "Finish called for NCS with id " << ncsId << endl;

    // record NCS statistics

    mwArray mw_costs;
    mwArray mw_ncsStats;

    ncs_finalize(2, mw_costs, mw_ncsStats, this->ncsHandle);

    this->recordScalar("total_control_costs", (double) mw_costs);

    // the stats should be a given as a single struct
    ASSERT(mw_ncsStats.NumberOfElements() == 1);
    ASSERT(mw_ncsStats.ClassID() == mxSTRUCT_CLASS);

    for (int i = 0; i < mw_ncsStats.NumberOfFields(); ++i) {
        const char* fieldName = (const char*) mw_ncsStats.GetFieldName(i);

        this->recordStatistics(mw_ncsStats(fieldName, 1, 1), fieldName);
    }
}

void NcsContext::handleMessage(cMessage * const msg) {
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
        case NCTXMK_TICKER_EVT: {
            // call loop with current timestamp, adjusted for the startup delay
            // thus, the matlab code never needs to deal with the time offset
            int64_t currentSimtime = (simTime() - startupDelay).inUnit(SIMTIME_PS);
            mwArray mw_ncsPktList;
            mwArray mw_ncsStats;
            mwArray mw_timestamp(currentSimtime);

            ncs_doLoopStep(2, mw_ncsPktList, mw_ncsStats, ncsHandle, mw_timestamp);

            sendNcsPktList(mw_ncsPktList);

            // signal updated statistical data
            emit(scSentSignal, static_cast<bool>(mw_ncsStats("sc_sent", 1, 1)));
            emit(caSentSignal, static_cast<bool>(mw_ncsStats("ca_sent", 1, 1)));
            emit(acSentSignal, static_cast<bool>(mw_ncsStats("ac_sent", 1, 1)));
            emit(qocSignal, static_cast<double>(mw_ncsStats("actual_qoc", 1, 1)));

            mwArray mw_scDelays = mw_ncsStats("sc_delays", 1, 1);
            mwArray mw_caDelays = mw_ncsStats("ca_delays", 1, 1);
            mwArray mw_acDelays = mw_ncsStats("ac_delays", 1, 1);
            const size_t scCount = mw_scDelays.NumberOfElements();
            const size_t caCount = mw_caDelays.NumberOfElements();
            const size_t acCount = mw_acDelays.NumberOfElements();

            for (size_t i = 1; i <= scCount; i++) {
                emit(scObservedDelaySignal, static_cast<uint64_t>(mw_scDelays(i)) * tickerInterval);
            }
            for (size_t i = 1; i <= caCount; i++) {
                emit(caObservedDelaySignal, static_cast<uint64_t>(mw_caDelays(i)) * tickerInterval);
            }
            for (size_t i = 1; i <= acCount; i++) {
                emit(acObservedDelaySignal, static_cast<uint64_t>(mw_acDelays(i)) * tickerInterval);
            }

            // reschedule ticker-event for next step
            scheduleAt(simTime() + tickerInterval, msg);

            break; }
        default:
            const int msgKind = msg->getKind();

            delete msg;

            error("Received self-message with unexpected message kind: %i", msgKind);
        }
    } else if (msg->arrivedOn((ACTUATOR_GATE + "$i").c_str())
                    || msg->arrivedOn((CONTROLLER_GATE + "$i").c_str())
                    || msg->arrivedOn((SENSOR_GATE + "$i").c_str())) {
        // packet arriving at a CPS gate

        RawPacket * const rawPkt = dynamic_cast<RawPacket *>(msg);

        if (!rawPkt) {
            error("Received unexpected packet kind at CPS in gate");
        }

        mwArray mw_pkt = createNcsPkt(rawPkt);

        // collect delay statistics for S->C, C->A, A->C
        const simtime_t pktDelay = simTime() - getNcsPktTimestamp(mw_pkt);

        if (rawPkt->arrivedOn((CONTROLLER_GATE + "$i").c_str())) {
            if (ncsPktIsAck(mw_pkt)) {
                // ACK packet sent back from actuator
                emit(acActualDelaySignal, pktDelay);
            } else {
                // regular data packet from sensor to controller
                emit(scActualDelaySignal, pktDelay);
            }
        } else {
            emit(caActualDelaySignal, pktDelay);
        }

        delete msg; // message will not be required any more, free it

        const int64_t currentSimtime = (simTime() - startupDelay).inUnit(SIMTIME_PS);
        mwArray mw_ncsPktList;
        mwArray mw_timestamp(currentSimtime);

        // forward to MATLAB/NCS model
        ncs_doHandlePacket(1, mw_ncsPktList, ncsHandle, mw_timestamp, mw_pkt);

        // and push replies back into the network
        sendNcsPktList(mw_ncsPktList);
    } else {
        const char * const name = msg->getName();

        delete msg;

        error("Received unexpected message: %s", name);
    }
}


std::vector<const char *> NcsContext::getConfigFieldNames() {
    return std::vector<const char *>();
}

void NcsContext::setConfigValues(mwArray &cfgStruct) {
    // nop.

    //Usage example
    //    mwArray mw_configStruct(1, 1, 1, fieldnames);
    //    mwArray mw_seqLength(42); //cfgStruct("controlSequenceLength", 1, 1);
    //    cfgStruct("controlSequenceLength", 1, 1) = mw_seqLength;
    //    //mwArray mw_maxMeasDelay(42);
    //    //cfgStruct("maxMeasDelay", 1, 1).Set(mw_maxMeasDelay);
}

void NcsContext::testNonnegLong(std::vector<const char *> &fieldNames, const char * name) {
    if (par(name).longValue() >= 0) {
        fieldNames.push_back(name);
    }
}

void NcsContext::testNonnegDbl(std::vector<const char *> &fieldNames, const char * name) {
    if (par(name).doubleValue() >= 0) {
        fieldNames.push_back(name);
    }
}

void NcsContext::testNonemptyDblVect(std::vector<const char *> &fieldNames, const char * name) {
    if (par(name).stdstringValue().length() > 0) {
        cStringTokenizer tokens(par(name).stringValue());
        const std::vector<double> dbls = tokens.asDoubleVector();
        const size_t values = dbls.size();

        if (values > 0) {
            fieldNames.push_back(name);
        }
    }
}

void NcsContext::setNonnegLong(mwArray &cfgStruct, const char * name) {
    if (par(name).longValue() >= 0) {
        const mwArray parValue(par(name).longValue());

        cfgStruct(name, 1, 1).Set(parValue);
    }
}

void NcsContext::setNonnegDbl(mwArray &cfgStruct, const char * name) {
    if (par(name).doubleValue() >= 0) {
        const mwArray parValue(par(name).doubleValue());

        cfgStruct(name, 1, 1).Set(parValue);
    }
}

void NcsContext::setNonemptyDblVect(mwArray &cfgStruct, const char * name) {
    if (par(name).stdstringValue().length() > 0) {
        cStringTokenizer tokens(par(name).stringValue());
        const std::vector<double> dbls = tokens.asDoubleVector();
        const size_t values = dbls.size();

        if (values > 0) {
            mwArray mwValues(1, &values, mxDOUBLE_CLASS);

            mwValues.SetData(const_cast<double *>(dbls.data()), values);

            cfgStruct(name, 1, 1).Set(mwValues);
        }
    }
}

void NcsContext::connect(const NcsContextComponentIndex dst) {
    ASSERT(dst >= 0 && dst < NCTXCI_COUNT);

    cMessage * const msg = new cMessage("ControllerConnectionRequest", CpsConnReq);
    NcsConnReq * const req = new NcsConnReq();

    req->setDstAddr(cpsAddr[dst]);

    msg->setControlInfo(req);

    send(msg, (*GATE_NAMES[NCTXCI_CONTROLLER] + "$o").c_str());
}

void NcsContext::recordStatistics(const mwArray& statistics, const std::string& statName) {
    mwArray dims = statistics.GetDimensions();
    uint32_t numRows = dims(1);

    cOutVector** statsVecs = new cOutVector*[numRows];
    const std::string name = statName;

    if (numRows == 1) {
        statsVecs[0] = new cOutVector(name.c_str());
        statsVecs[0]->setType(cOutVector::TYPE_DOUBLE);
    } else {
        for (uint32_t i= 0; i < numRows; ++i) {
            statsVecs[i] = new cOutVector((statName + "(" + std::to_string(i+1) + ")").c_str());
            statsVecs[i]->setType(cOutVector::TYPE_DOUBLE);
        }
    }
    uint32_t numElements = dims(2);
    for (uint32_t i = 0; i < numElements; ++i) {
        const simtime_t time = this->tickerInterval * i + startupDelay;

        for (uint32_t j = 0; j < numRows; ++j) {
            statsVecs[j]->recordWithTimestamp(time, (double) statistics(j + 1, i + 1));
        }
    }
    for (uint32_t i= 0; i < numRows; ++i) {
        delete statsVecs[i];
    }

    delete[] statsVecs;
}

mwArray NcsContext::createNcsConfigStruct() {
    const std::vector<const char *> fields = getConfigFieldNames();

    mwArray result = fields.size() > 0 ? mwArray(1, 1, fields.size(), const_cast<const char **>(fields.data())) : mwArray(mxSTRUCT_CLASS);

    setConfigValues(result);

    return result;
}

mwArray NcsContext::createNcsPkt(RawPacket* const msg) {
    // transform the OMNeT++/INET RawPacket to a MATLAB NCS packet

    NcsSendData* const req = dynamic_cast<NcsSendData*>(msg->getControlInfo());

    const NcsContextComponentIndex srcIndex = getIndexForAddr(req->getSrcAddr());
    const NcsContextComponentIndex dstIndex = getIndexForAddr(req->getDstAddr());

    EV << "ctx payload in from " << srcIndex << " to " << dstIndex << endl;

    ASSERT(srcIndex < NCTXCI_COUNT);
    ASSERT(dstIndex < NCTXCI_COUNT);

    const size_t payloadSize = msg->getByteArray().getDataArraySize();

    EV << "Received payload of size " << payloadSize << " from raw packet" << endl;

    mwArray mw_payload(1, &payloadSize, mxUINT8_CLASS);

    mw_payload.SetData(reinterpret_cast<mxUint8 *>(msg->getByteArray().getDataPtr()), payloadSize);

    EV << "Attempt to create Matlab DataPacket from raw packet" << endl;

    mwArray mw_src(srcIndex);
    mwArray mw_dst(dstIndex);
    mwArray mw_pkt;

    ncs_pktCreate(1, mw_pkt, mw_src, mw_dst, mw_payload);

    return mw_pkt;
}

simtime_t NcsContext::getNcsPktTimestamp(const mwArray& mw_pkt) {
    mwArray mw_timestamp;

    ncs_pktGetTimeStamp(1, mw_timestamp, mw_pkt);

    return tickerInterval * static_cast<uint64_t>(mw_timestamp) + startupDelay;
}

bool NcsContext::ncsPktIsAck(const mwArray& mw_pkt) {
    mwArray mw_isAck;

    ncs_pktIsAck(1, mw_isAck, mw_pkt);

    return static_cast<bool>(mw_isAck);
}

RawPacket* NcsContext::parseNcsPkt(const mwArray& mw_pkt) {
    // transform the MATLAB NCS packet into an OMNeT++/INET RawPacket

    ASSERT(mw_pkt.NumberOfElements() == 1);

    mwArray mw_src, mw_dst, mw_payload;

    ncs_pktGetSrcAddr(1, mw_src, mw_pkt);
    ncs_pktGetDstAddr(1, mw_dst, mw_pkt);
    ncs_pktGetPayload(1, mw_payload, mw_pkt);

    // the payload should be a byte stream (row vector like 1-by-n)
    ASSERT(mw_payload.NumberOfDimensions() == 2);
    ASSERT((size_t ) mw_payload.GetDimensions()(1) == 1);
    ASSERT(mw_payload.ClassID() == mxUINT8_CLASS);

    RawPacket* const rawPkt = new RawPacket("CPS Payload", CpsSendData);
    NcsSendData* const req = new NcsSendData();
    ByteArray payloadArray;
    uint8_t payloadBuf[mw_payload.NumberOfElements()];

    mw_payload.GetData(payloadBuf, sizeof(payloadBuf));
    payloadArray.setDataFromBuffer(payloadBuf, sizeof(payloadBuf));

    const int srcIndex = mw_src;
    const int dstIndex = mw_dst;

    ASSERT(srcIndex < NCTXCI_COUNT);
    ASSERT(dstIndex < NCTXCI_COUNT);

    req->setSrcAddr(cpsAddr[srcIndex]);
    req->setDstAddr(cpsAddr[dstIndex]);

    rawPkt->setByteLength(sizeof(payloadBuf));
    rawPkt->setByteArray(payloadArray);
    rawPkt->setControlInfo(req);

    return rawPkt;
}

void NcsContext::sendNcsPkt(const mwArray& mw_pkt) {
    // send NCS packet from MATLAB via the matching CPS into the network

    RawPacket* const rawPkt = parseNcsPkt(mw_pkt);
    NcsSendData* const req = dynamic_cast<NcsSendData*>(rawPkt->getControlInfo());

    const int srcIndex = getIndexForAddr(req->getSrcAddr());
    const int dstIndex = getIndexForAddr(req->getDstAddr());

    EV << rawPkt->getByteLength() << " bytes ctx payload out from " << srcIndex << " to " << dstIndex << endl;

    send(rawPkt, (*GATE_NAMES[srcIndex] + "$o").c_str()); // forward pkt to sending CPS
}

void NcsContext::sendNcsPktList(const mwArray& mw_ncsPktList) {
    // transform obtained byte arrays to raw packet and send

    size_t cpsPktCount = mw_ncsPktList.NumberOfElements();

    if (cpsPktCount != 0) {
        // mw_cpsPktList with packets to send to a certain node should be a
        // cpsPktCount-by-1 cell array (column-vector like)
        ASSERT(mw_ncsPktList.NumberOfDimensions() == 2);
        ASSERT((size_t ) mw_ncsPktList.GetDimensions()(1) == cpsPktCount);
        ASSERT((size_t ) mw_ncsPktList.GetDimensions()(2) == 1);
        ASSERT(mw_ncsPktList.ClassID() == mxCELL_CLASS);

        for (uint32_t pktNum = 0; pktNum < cpsPktCount; pktNum++) {
            sendNcsPkt(mw_ncsPktList(pktNum + 1));
        }
    }
}

NcsContextComponentIndex NcsContext::getIndexForAddr(const L3Address &addr) {
    for (int i = NCTXCI_ACTUATOR; i < NCTXCI_COUNT; i++) {
        if (cpsAddr[i] == addr) {
            return static_cast<NcsContextComponentIndex>(i);
        }
    }

    return NCTXCI_COUNT;
}

