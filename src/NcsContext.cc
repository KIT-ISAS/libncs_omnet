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

#include "NcsContext.h"
#include "NcsCpsApp.h"
#include "messages/NcsCtrlMsg_m.h"

#include <inet/common/InitStages.h>
#include <inet/networklayer/common/L3AddressResolver.h>

Define_Module(NcsContext);

const std::string NcsContext::NCS_ACTUATOR = "actuator";
const std::string NcsContext::NCS_CONTROLLER = "controller";
const std::string NcsContext::NCS_SENSOR = "sensor";
const std::string * NcsContext::NCS_NAMES[] = { &NCS_ACTUATOR, &NCS_CONTROLLER, &NCS_SENSOR };

int NcsContext::ncsIdCounter = 1;



NcsContext::~NcsContext() {
    if (ncsParameters) {
        delete ncsParameters;
    }

    if (ncsSignals) {
        delete ncsSignals;
    }

    if (ncsControlStepResult) {
        delete ncsControlStepResult;
    }

    if (ncsPlantStepResult) {
        delete ncsPlantStepResult;
    }
}

int NcsContext::numInitStages() const {
    return INITSTAGE_LAST;
}

void NcsContext::initialize(const int stage) {
    switch (stage) {
    case INITSTAGE_LOCAL:
        // reset at the beginning of each simulation to get the same IDs for repeated runs
        ncsIdCounter = 1;

        // get own parameters
        ncsImplName = par("ncsImpl").stdstringValue();
        networkStartupPollInterval = par ("networkStartupPollInterval").doubleValue();
        startupDelay = par("startupDelay").doubleValue();
        simulationRuntime = par("simulationRuntime").doubleValue();
        actionOnControllerFailure = par("actionOnControllerFailure").intValue();
        minSampleCount = par("minSampleCount").intValue();
        maxSampleCount = par("maxSampleCount").intValue();
        maxSampleAge = par("maxSampleAge").doubleValue();
        maxSampleBins = par("maxSampleBins").intValue();
        reportUnusedStepsAsLoss = par("reportUnusedStepsAsLoss").boolValue();
        pktStatisticsStartDelay = par("pktStatisticsStartDelay").doubleValue();

        // setup signals for statistics recording
        scSentSignal = registerSignal("sc_sent");
        caSentSignal = registerSignal("ca_sent");
        acSentSignal = registerSignal("ac_sent");

        scActualDelaySignal = registerSignal("sc_delay_act");
        caActualDelaySignal = registerSignal("ca_delay_act");
        acActualDelaySignal = registerSignal("ac_delay_act");

        controlPeriodSignal = registerSignal("controlPeriod_s");
        break;
    case INITSTAGE_LOCAL + 1: {
        ncsId = ncsIdCounter++; // draw an identifier

        // Prepare Parameters for NCS
        ncsParameters = createParameters();

        // Prepare Signals for NCS
        ncsSignals = createSignals();

        // initially create step result struct
        ncsControlStepResult = createControlStepResult();
        ncsPlantStepResult = createPlantStepResult();

        // everything is set up right now, create and initialize NCS
        ncs = createNcsImpl(ncsImplName);
        ncs->initializeNcs(this);

        // upon initialization, we can assume that the plant state is admissible
        plantStateAdmissible = true;
        controllerStateAdmissible = true;

        // setup periodic ticker event for NCS data processing
        controlPeriod = ncs->getControlPeriod();
        plantPeriod = ncs->getPlantPeriod();
        nextControlStep = startupDelay + controlPeriod;
        nextPlantStep = startupDelay + plantPeriod;

        cMessage * const tickerMsg = new cMessage("NcsTickerEvent", NCTXMK_TICKER_EVT);

        scheduleAt(std::min(nextPlantStep, nextControlStep), tickerMsg);

        if (pktStatisticsStartDelay > SIMTIME_ZERO) {
            cMessage * const statisticsStartup = new cMessage("NcsPktStatisticsStartEvent", NCTXMK_STARTUP_STATS_EVT);

            scheduleAt(pktStatisticsStartDelay, statisticsStartup);
        }

        emit(controlPeriodSignal, controlPeriod);

        } break;
    case INITSTAGE_APPLICATION_LAYER: {
        networkConfigured = false;

        if (networkStartupPollInterval != SIMTIME_ZERO) {
            cMessage * const tickerMsg = new cMessage("NcsStartupPollTicker", NCTXMK_STARTUP_POLL_EVT);

            scheduleAt(networkStartupPollInterval, tickerMsg);
        } else {
            if (!setupNCSConnections()) {
                error("Network was not fully set up during initialization, MatlabNcsContext is unable to operate. Network startup poll interval must be configured.");
            }
        }
        } break;
    }
}

void NcsContext::finish() {
    EV << "Finish called for NCS with id " << ncsId << endl;

    finishNcs();

    // record NCS statistics and do potential cleanup
    ncs->finishNcs();
}

void NcsContext::finishNcs() {
    emit(registerSignal("sen_pktsSent_s"), scHist.pktsSent());
    emit(registerSignal("sen_pktsArrived_s"), scHist.pktsArrived());
    emit(registerSignal("sen_pktsLost_s"), scHist.pktsLost());
    emit(registerSignal("con_pktsSent_s"), caHist.pktsSent());
    emit(registerSignal("con_pktsArrived_s"), caHist.pktsArrived());
    emit(registerSignal("con_pktsLost_s"), caHist.pktsLost());
    emit(registerSignal("act_pktsSent_s"), scHist.pktsSent());
    emit(registerSignal("act_pktsArrived_s"), scHist.pktsArrived());
    emit(registerSignal("act_pktsLost_s"), scHist.pktsLost());

    emit(registerSignal("plantStateAdmissible_s"), plantStateAdmissible);
    emit(registerSignal("controllerStateAdmissible_s"), controllerStateAdmissible);
}

void NcsContext::handleMessage(cMessage * const msg) {
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
        case NCTXMK_TICKER_EVT: {
            const simtime_t now = simTime();
            // call loop with current timestamp, adjusted for the startup delay
            // thus, NCS code never needs to deal with the time offset
            const simtime_t ncsSimtime = now - startupDelay;

            if (simulationRuntime > SIMTIME_ZERO && ncsSimtime > simulationRuntime) {
                delete msg;

                EV_INFO << "runtime limit reached for NCS, stopping periodic ticker" << endl;

                ncsRuntimeLimitReached();

                return;
            }

            if (now == nextPlantStep) {
                doPlantStep(ncsSimtime);

                nextPlantStep += plantPeriod;
            } else {
                ASSERT(now == nextControlStep);

                // update delay histogram data
                scHist.prune(now, maxSampleAge, minSampleCount, maxSampleCount);
                caHist.prune(now, maxSampleAge, minSampleCount, maxSampleCount);
                acHist.prune(now, maxSampleAge, minSampleCount, maxSampleCount);

                doControlStep(ncsSimtime);

                nextControlStep += controlPeriod;
            }

            // reschedule ticker-event for next step
            scheduleAt(std::min(nextPlantStep, nextControlStep), msg);

            break; }
        case NCTXMK_STARTUP_POLL_EVT:
            if (!setupNCSConnections()) {
                EV_WARN << "Network is not ready yet, retrying again later" << endl;
                // reschedule polling event for next try
                scheduleAt(simTime() + networkStartupPollInterval, msg);
            } else {
                delete msg;
            }
            break;
        case NCTXMK_STARTUP_STATS_EVT:
            scHist.resetStats();
            caHist.resetStats();
            acHist.resetStats();

            delete msg;
            break;
        default:
            const int msgKind = msg->getKind();

            delete msg;

            error("Received self-message with unexpected message kind: %i", msgKind);
        }
    } else if (msg->arrivedOn((NCS_ACTUATOR + "$i").c_str())
                    || msg->arrivedOn((NCS_CONTROLLER + "$i").c_str())
                    || msg->arrivedOn((NCS_SENSOR + "$i").c_str())) {
        // packet arriving at a CPS gate
        switch (msg->getKind()) {
        case CpsConnReq: {
            NcsConnReq * const req = dynamic_cast<NcsConnReq *>(msg->removeControlInfo());

            const NcsContextComponentIndex index = getIndexForAddr(req->getDstAddr());

            ASSERT(index < NCTXCI_COUNT);

            postConnect(index);

            delete req;
            delete msg;
            break; }
        default:
            RawPacket * const rawPkt = dynamic_cast<RawPacket *>(msg);

            if (!rawPkt) {
                error("Received unexpected packet kind at CPS in gate");
            }

            handleNcsPacketFromNetwork(rawPkt);

            delete rawPkt;
        }
    } else {
        const char * const name = msg->getName();

        delete msg;

        error("Received unexpected message: %s", name);
    }
}

NcsContext::NcsDelays NcsContext::computeDelays() {
    NcsContext::NcsDelays result;
    const simtime_t now = simTime();

    result.sc = scHist.compute(now, controlPeriod, maxSampleBins);
    result.ca = caHist.compute(now, controlPeriod, maxSampleBins);

    return result;
}

void NcsContext::updateControlPeriod(const simtime_t newControlPeriod) {
    ASSERT(simTime() == nextControlStep);

    controlPeriod = newControlPeriod;

    emit(controlPeriodSignal, newControlPeriod);
}

NcsContext::NcsParameters* NcsContext::createParameters(NcsParameters * const parameters) {
    NcsParameters * const result = (parameters != nullptr) ? parameters : new NcsParameters();

    result->ncsId = ncsId;
    result->startupDelay = startupDelay;
    result->simTimeLimit = SimTime::parse(cSimulation::getActiveEnvir()->getConfig()->getConfigEntry("sim-time-limit").getValue());
    result->configFile =  &par("configFile");
    result->controllerClassName = &par("controllerClassName");
    result->filterClassName = &par("filterClassName");
    result->networkType = &par("networkType");
    result->controlSequenceLength = &par("controlSequenceLength");
    result->maxMeasDelay = &par("maxMeasDelay");
    result->mpcHorizon = &par("mpcHorizon");
    result->controlErrorWindowSize = &par("controlErrorWindowSize");

    return result;
}

NcsContext::NcsSignals* NcsContext::createSignals(NcsSignals * const signals) {
    NcsSignals * const result = (signals != nullptr) ? signals : new NcsSignals();

    result->controlErrorSignal = registerSignal("act_control_error");
    result->estControlErrorSignal = registerSignal("est_control_error");
    result->stageCostsSignal = registerSignal("act_stage_costs");
    result->scObservedDelaySignal = registerSignal("sc_delay_obs");
    result->caObservedDelaySignal = registerSignal("ca_delay_obs");
    result->acObservedDelaySignal = registerSignal("ac_delay_obs");

    return result;
}

NcsContext::NcsControlStepResult* NcsContext::createControlStepResult() {
    return new NcsControlStepResult();
}

NcsContext::NcsPlantStepResult* NcsContext::createPlantStepResult() {
    return new NcsPlantStepResult();
}

AbstractNcsImpl* NcsContext::createNcsImpl(const std::string name) {
    cModuleType * const moduleType = cModuleType::get(name.c_str());

    cModule * const module = moduleType->createScheduleInit("ncs", this);

    AbstractNcsImpl * const result = dynamic_cast<AbstractNcsImpl * const>(module);

    if (result == nullptr) {
        delete module;

        error("Failed to instantiate NCS implementation %s", name);
    }

    return result;
}

void NcsContext::doPlantStep(const simtime_t& ncsTime) {
    ncs->doPlantStep(ncsTime, ncsPlantStepResult);

    processPlantStepResult(ncsTime, ncsPlantStepResult);
}

void NcsContext::doControlStep(const simtime_t& ncsTime) {
    ncs->doControlStep(ncsTime, ncsControlStepResult);

    processControlStepResult(ncsTime, ncsControlStepResult);
}

void NcsContext::processPlantStepResult(const simtime_t& ncsTime, const NcsPlantStepResult * const result) {
    if (!result->plantStateAdmissible) {
        plantStateAdmissible = false;

        handleControllerFailure();
    }
}

void NcsContext::processControlStepResult(const simtime_t& ncsTime, const NcsControlStepResult * const result) {
    if (!result->controllerStateAdmissible) {
        controllerStateAdmissible = false;

        handleControllerFailure();
    }

    CommunicationStatus cs = sendNcsPacketsToNetwork(result->pkts);

    // report control sequence as lost, if event-trigger decided to suppress it
    if (reportUnusedStepsAsLoss && !cs.ca) {
        caHist.sent(-1, simTime(), true);
    }
}

void NcsContext::handleControllerFailure() {
    switch (actionOnControllerFailure) {
    case NCTXCFA_FINISH:
        endSimulation();
        break;
    case NCTXCFA_ABORT:
        error("Plant or controller left admissible state at time %s", simTime().str());
        break;
    default: // ignore
        break;
    }
}

NcsContext::CommunicationStatus NcsContext::sendNcsPacketsToNetwork(const std::vector<NcsPkt> pkts) {
    CommunicationStatus cs = CommunicationStatus{false, false, false};

    if (networkConfigured) {
        for (auto &pkt : pkts) {
            sendNcsPacketToNetwork(pkt, cs);
        }
    } else {
        EV_WARN << "Network did not finish initialization at time t=" << simTime().str()
                << ". Dropping all NCS communication of NCS " << ncsId << " for this time step." << endl;
    }

    // signal updated statistical data
    emit(scSentSignal, cs.sc);
    emit(caSentSignal, cs.ca);
    emit(acSentSignal, cs.ac);

    return cs;
}

void NcsContext::sendNcsPacketToNetwork(const NcsPkt& ncsPkt, CommunicationStatus& cs) {
    // send NCS packet via the matching CPS into the network

    RawPacket* const rawPkt = ncsPkt.pkt;
    NcsSendData* const req = new NcsSendData();

    const unsigned int srcIndex = ncsPkt.src;
    const unsigned int dstIndex = ncsPkt.dst;

    ASSERT(srcIndex < NCTXCI_COUNT);
    ASSERT(dstIndex < NCTXCI_COUNT);

    req->setSrcAddr(cpsAddr[srcIndex]);
    req->setDstAddr(cpsAddr[dstIndex]);

    rawPkt->setName("CPS Payload");
    rawPkt->setKind(CpsSendData);
    rawPkt->setControlInfo(req);

    const uint64_t pktId = ncsPkt.pktId;

    switch (srcIndex) {
    case NCTXCI_ACTUATOR:
        // actuator->controller
        acHist.sent(pktId, simTime());
        cs.ac = true;
        break;
    case NCTXCI_CONTROLLER:
        // controller->actuator
        caHist.sent(pktId, simTime());
        cs.ca = true;
        break;
    case NCTXCI_SENSOR:
        // sensor-->controller
        scHist.sent(pktId, simTime());
        cs.sc = true;
        break;
    default:
        ASSERT(false);
    }

    EV << "pktId " << pktId << ": " << rawPkt->getByteLength()
            << " bytes ctx payload out from " << NCS_NAMES[srcIndex]->c_str()
            << " to " << NCS_NAMES[dstIndex]->c_str() << endl;

    send(rawPkt, (*NCS_NAMES[srcIndex] + "$o").c_str()); // forward pkt to sending CPS
}

void NcsContext::handleNcsPacketFromNetwork(RawPacket* const rawPkt) {
    NcsSendData * const info = dynamic_cast<NcsSendData *>(rawPkt->getControlInfo());
    const size_t payloadSize = rawPkt->getByteArray().getDataArraySize();


    ASSERT(info);

    const simtime_t now = simTime();
    const simtime_t ncsSimtime = (now - startupDelay);

    NcsPkt ncsPkt = NcsPkt{
        getIndexForAddr(info->getSrcAddr()),
        getIndexForAddr(info->getDstAddr()),
        0, // unknown
        false, // to be computed
        rawPkt
    };
    ncsPkt.isAck = (ncsPkt.src == NCTXCI_ACTUATOR) && (ncsPkt.dst == NCTXCI_CONTROLLER);

    EV << "raw packet with " << payloadSize << " bytes ctx payload in from "
                << NCS_NAMES[ncsPkt.src]->c_str() << " to " << NCS_NAMES[ncsPkt.dst]->c_str() << endl;


    // forward to NCS model
    std::vector<NcsContext::NcsPkt> pktList = ncs->handlePacket(ncsSimtime, ncsPkt);

    const simtime_t pktDelay = now - rawPkt->getCreationTime();
    const uint64_t pktId = ncsPkt.pktId;

    if (ncsPkt.dst == NCTXCI_ACTUATOR) {
        emit(caActualDelaySignal, pktDelay);
        caHist.received(pktId, now);
    } else if (ncsPkt.dst == NCTXCI_CONTROLLER) {
        if (!ncsPkt.isAck) {
            // regular data packet from sensor to controller
            emit(scActualDelaySignal, pktDelay);
            scHist.received(pktId, now);
        } else {
            // ACK packet sent back from actuator
            emit(acActualDelaySignal, pktDelay);
            acHist.received(pktId, now);
        }
    }

    EV_INFO << "pktId " << pktId << " in with delay " << pktDelay << endl;

    // and push replies back into the network
    if (pktList.size() > 0) {
        sendNcsPacketsToNetwork(pktList);
    }
}

bool NcsContext::setupNCSConnections() {
    // IP auto assignment, lookup address by name
    // self.prefix + "controller|actuator|sensor"
    cModule* const parent = getParentModule();

    ASSERT(parent);

    const std::string parentPath = parent->getFullPath();

    L3AddressResolver().tryResolve((parentPath + "." + NCS_ACTUATOR).c_str(),
            cpsAddr[NCTXCI_ACTUATOR],
            L3AddressResolver::ADDR_IPv4 | L3AddressResolver::ADDR_IPv6);
    L3AddressResolver().tryResolve((parentPath + "." + NCS_CONTROLLER).c_str(),
            cpsAddr[NCTXCI_CONTROLLER],
            L3AddressResolver::ADDR_IPv4 | L3AddressResolver::ADDR_IPv6);
    L3AddressResolver().tryResolve((parentPath + "." + NCS_SENSOR).c_str(),
            cpsAddr[NCTXCI_SENSOR],
            L3AddressResolver::ADDR_IPv4 | L3AddressResolver::ADDR_IPv6);

    if (cpsAddr[NCTXCI_ACTUATOR].isUnspecified()) {
        error(("Unable to resolve address of actuator for NCS " + parentPath).c_str());
    }
    if (cpsAddr[NCTXCI_CONTROLLER].isUnspecified()) {
        error(("Unable to resolve address of controller for NCS " + parentPath).c_str());
    }
    if (cpsAddr[NCTXCI_SENSOR].isUnspecified()) {
        error(("Unable to resolve address of sensor for NCS " + parentPath).c_str());
    }

    if (cpsAddr[NCTXCI_ACTUATOR].isLinkLocal()
            || cpsAddr[NCTXCI_CONTROLLER].isLinkLocal()
            || cpsAddr[NCTXCI_SENSOR].isLinkLocal()) {
        // we will not rely on link local adresses
        // routing will not work in case of IPv6
        return false;
    }

    EV << parent->getFullPath() << "." << NCS_ACTUATOR << " is " << cpsAddr[NCTXCI_ACTUATOR].str() << endl;
    EV << parent->getFullPath() << "." << NCS_CONTROLLER << " is " << cpsAddr[NCTXCI_CONTROLLER].str() << endl;
    EV << parent->getFullPath() << "." << NCS_SENSOR << " is " << cpsAddr[NCTXCI_SENSOR].str() << endl;

    // connect Controller to Actuator and Sensor
    connect(NCTXCI_ACTUATOR);
    connect(NCTXCI_SENSOR);

    networkConfigured = true;

    postNetworkInit();

    return networkConfigured;
}

void NcsContext::connect(const NcsContextComponentIndex dst) {
    ASSERT(dst >= 0 && dst < NCTXCI_COUNT);

    cMessage * const msg = new cMessage("ControllerConnectionRequest", CpsConnReq);
    NcsConnReq * const req = new NcsConnReq();

    req->setDstAddr(cpsAddr[dst]);

    msg->setControlInfo(req);

    send(msg, (*NCS_NAMES[NCTXCI_CONTROLLER] + "$o").c_str());
}

NcsContextComponentIndex NcsContext::getIndexForAddr(const L3Address &addr) {
    for (int i = NCTXCI_ACTUATOR; i < NCTXCI_COUNT; i++) {
        if (cpsAddr[i] == addr) {
            return static_cast<NcsContextComponentIndex>(i);
        }
    }

    return NCTXCI_COUNT;
}
