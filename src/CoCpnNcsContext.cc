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

#include "CoCpnNcsContext.h"

#include "NcsCpsApp.h"
#include "CoCC/CoCCUDPTransport.h"
#include "messages/NcsCtrlMsg_m.h"

#include <utility>
#include <inet/common/InitStages.h>
#include <omnetpp/cstringtokenizer.h>

Define_Module(CoCpnNcsContext);


#define NEWTON_EPSILON 1E-8
#define NEWTON_ITER_LIMIT 10
#define BISECT_ITER_LIMIT 5
#define DIFF_H 1E-5


void CoCpnNcsContext::initialize(const int stage) {
    NcsContext::initialize(stage);

    switch (stage) {
    case INITSTAGE_LOCAL:
        // get own parameters
        payloadSize = par("payloadSize").intValue();
        autoPayloadSize = par("autoPayloadSize").boolValue();
        useSampledDelayProbs = par("useSampledDelayProbs").boolValue();

        factor = par("factor").doubleValue();
        offset = par("offset").doubleValue();

        if (qocToQM(1.0) < 1.0) {
            EV_WARN << "WARNING: QoC<->QM mapping is configured such that QM=1 can never be achived. factor=" << factor << " offset=" << offset << endl;
        }

        // setup signals for statistics recording
        reportedQoC = registerSignal("reportedQoC");
        reportedQM = registerSignal("reportedQM");
        targetQoCSignal = registerSignal("targetQoC");
        targetQMSignal = registerSignal("targetQM");
        sPayloadSize = registerSignal("sPayloadSize");
        break;
    }
}

void CoCpnNcsContext::finish() {
    if (simulationRuntime == SIMTIME_ZERO || simTime() < startupDelay + simulationRuntime) {
        emit(targetQoCSignal, qmToQoC(this->targetQM));
        emit(targetQMSignal, this->targetQM);
    }

    NcsContext::finish();
}

void CoCpnNcsContext::postNetworkInit() {
    // set Translator for controller

    NcsSetTranslator * const tCtrl = new NcsSetTranslator();

    tCtrl->setTranslator(this);
    tCtrl->setRole(NCTXCI_CONTROLLER);
    tCtrl->setDstAddr(cpsAddr[NCTXCI_ACTUATOR]);

    cMessage * const tMsg = new cMessage("CoCPN translator pushdown", CpsTranslator);

    tMsg->setControlInfo(tCtrl);

    send(tMsg, (*NCS_NAMES[NCTXCI_CONTROLLER] + "$o").c_str());

    // set stream start

    NcsStreamStartInfo * const startCtrl = new NcsStreamStartInfo();

    startCtrl->setStart(startupDelay);
    startCtrl->setDstAddr(cpsAddr[NCTXCI_ACTUATOR]);

    cMessage * const startMsg = new cMessage("CoCPN stream start pushdown", CpsStreamStart);

    startMsg->setControlInfo(startCtrl);

    send(startMsg, (*NCS_NAMES[NCTXCI_CONTROLLER] + "$o").c_str());

    // set stream stop

    if (simulationRuntime > SIMTIME_ZERO) {
        NcsStreamStopInfo * const stopCtrl = new NcsStreamStopInfo();

        stopCtrl->setStop(startupDelay + simulationRuntime);
        stopCtrl->setDstAddr(cpsAddr[NCTXCI_ACTUATOR]);

        cMessage * const stopMsg = new cMessage("CoCPN stream stop pushdown", CpsStreamStop);

        stopMsg->setControlInfo(stopCtrl);

        send(stopMsg, (*NCS_NAMES[NCTXCI_CONTROLLER] + "$o").c_str());
    }
}

void CoCpnNcsContext::postConnect(const NcsContextComponentIndex to)  {
    // set translator to actuator. not possible until connection is established
    if (to == NCTXCI_ACTUATOR) {
        NcsSetTranslator * const tCtrl = new NcsSetTranslator();

        tCtrl->setTranslator(this);
        tCtrl->setRole(NCTXCI_ACTUATOR);
        tCtrl->setDstAddr(cpsAddr[NCTXCI_CONTROLLER]);

        cMessage * const tMsg = new cMessage("CoCPN translator pushdown", CpsTranslator);

        tMsg->setControlInfo(tCtrl);

        send(tMsg, (*NCS_NAMES[NCTXCI_ACTUATOR] + "$o").c_str());
    }
}

CoCpnNcsContext::CoCpnNcsParameters* CoCpnNcsContext::getParameters() {
    ASSERT(dynamic_cast<CoCpnNcsParameters*>(ncsParameters));

    return reinterpret_cast<CoCpnNcsParameters*>(ncsParameters);
}

CoCpnNcsContext::CoCpnNcsSignals* CoCpnNcsContext::getSignals() {
    ASSERT(dynamic_cast<CoCpnNcsSignals*>(ncsSignals));

    return reinterpret_cast<CoCpnNcsSignals*>(ncsSignals);
}

CoCpnNcsContext::CoCpnNcsParameters* CoCpnNcsContext::createParameters(NcsParameters * const parameters) {
    CoCpnNcsParameters * const result = (parameters != nullptr) ? dynamic_cast<CoCpnNcsParameters*>(parameters) : new CoCpnNcsParameters();

    ASSERT(result);

    NcsContext::createParameters(result);

    result->useSampledDelayProbs = useSampledDelayProbs;
    result->samplingInterval = &par("samplingInterval");
    result->scDelayProbs = &par("scDelayProbs");
    result->caDelayProbs = &par("caDelayProbs");
    result->controllerEventBased = &par("controllerEventBased");
    result->controllerEventTrigger = &par("controllerEventTrigger");
    result->controllerDeadband = &par("controllerDeadband");
    result->sensorEventBased = &par("sensorEventBased");
    result->sensorMeasDelta = &par("sensorMeasDelta");
    result->translatorFile = &par("translatorFile");

    return result;
}

CoCpnNcsContext::CoCpnNcsSignals* CoCpnNcsContext::createSignals(NcsSignals * const signals) {
    CoCpnNcsSignals * const result = (signals != nullptr) ? dynamic_cast<CoCpnNcsSignals*>(signals) : new CoCpnNcsSignals();

    ASSERT(result);

    NcsContext::createSignals(result);

    return result;
}

CoCpnNcsContext::CoCpnNcsControlStepResult* CoCpnNcsContext::createControlStepResult() {
    return new CoCpnNcsControlStepResult();
}

AbstractNcsImpl* CoCpnNcsContext::createNcsImpl(const std::string name) {
    cModuleType * const moduleType = cModuleType::get(name.c_str());

    cModule * const module = moduleType->createScheduleInit("ncs", this);

    AbstractCoCpnNcsImpl * const result = dynamic_cast<AbstractCoCpnNcsImpl * const>(module);

    if (result == nullptr) {
        delete module;

        error("Failed to instantiate CoCPN NCS implementation %s", name);
    }

    return result;
}

void CoCpnNcsContext::processControlStepResult(const simtime_t& ncsTime, const NcsControlStepResult * const result) {
    ASSERT(dynamic_cast<const CoCpnNcsControlStepResult*>(result));

    auto res = reinterpret_cast<const CoCpnNcsControlStepResult*>(result);

    if (autoPayloadSize) {
        for (auto &pkt : res->pkts) {
            if (pkt.src == NCTXCI_CONTROLLER && pkt.dst == NCTXCI_ACTUATOR) {
                avgPayloadSize = (payloadSizeSamples * avgPayloadSize + pkt.pkt->getByteLength()) / (payloadSizeSamples + 1);

                if (++payloadSizeSamples > 20) {
                    payloadSizeSamples /= 2;
                }
            }
        }
    }

    NcsContext::processControlStepResult(ncsTime, result);

    actualQoC = res->reportedQoC;

    emit(reportedQoC, actualQoC);
    emit(reportedQM, CLAMP(qocToQM(actualQoC), 0.0, 1.0));

    // notify the observer about the current step
    if (observer) {
        observer->postControlStep(observerContext);
    }
}

AbstractCoCpnNcsImpl* CoCpnNcsContext::ncs() {
    ASSERT(dynamic_cast<AbstractCoCpnNcsImpl*>(NcsContext::ncs));

    return reinterpret_cast<AbstractCoCpnNcsImpl*>(NcsContext::ncs);
}

simtime_t CoCpnNcsContext::getControlPeriod() {
    return controlPeriod;
}

void CoCpnNcsContext::setControlObserver(ICoCCTranslator::IControlObserver * const observer, void * const context) {
    this->observer = observer;
    this->observerContext = context;
}

double CoCpnNcsContext::getActualQM() {
    return qocToQM(actualQoC);
}

double CoCpnNcsContext::getTargetQM() {
    return targetQM;
}

void CoCpnNcsContext::setTargetQM(const double targetQM) {
    getParameters()->ignoreDeadband = true;

    // save targetQM
    this->targetQM = CLAMP(targetQM, 0.0, 1.0);
    const double targetQoC = qmToQoC(targetQM);

    ncs()->setTargetQoC(targetQoC);

    emit(targetQoCSignal, targetQoC);
    emit(targetQMSignal, this->targetQM);
}

long CoCpnNcsContext::getPayloadSize() {
    if (autoPayloadSize && payloadSizeSamples > 0) {
        const long size = std::round(avgPayloadSize);

        if (size != lastPayloadSize) {
            emit(sPayloadSize, size);

            EV_DEBUG << "autodetected payload size changed from " << lastPayloadSize << " to " << size << endl;

            lastPayloadSize = size;
        }

        return size;
    } else {
        return payloadSize;
    }
}

long CoCpnNcsContext::getPerPacketOverhead() {
    return perPacketOverhead;
}

void CoCpnNcsContext::setPerPacketOverhead(const long packetOverhead) {
    this->perPacketOverhead = packetOverhead;
}

long CoCpnNcsContext::getNetworkOverhead() {
    return networkOverhead;
}

void CoCpnNcsContext::setNetworkOverhead(const long networkOverhead) {
    this->networkOverhead = networkOverhead;
}

double CoCpnNcsContext::getMaxRate() {
    // TODO: This is a quirk. Maximum rate might be lower if QM<->QoC mapping is active.
    return (getPayloadSize() + perPacketOverhead) * 8 / controlPeriod.dbl() + networkOverhead * 8;
}

ICoCCTranslator::CoCCLinearization CoCpnNcsContext::getLinearizationForRate(const double actualQM, const double targetQM) {
    ICoCCTranslator::CoCCLinearization result;

    RateAdjustment function(this);

    FunctionTools::Derivative derive(function, DIFF_H);

    result.m = derive(targetQM);
    result.b = function(targetQM) - result.m * targetQM;

    ASSERT(result.m >= 0 - COCC_EPSILON); // negative slopes are not reasonable, catch them

    //EV_TRACE << "linearization for QM: " << result.m << " * QM + " << result.b << endl;
    //EV_TRACE << "rate according to linearization at target QM (" << targetQM << "): " << result.m * targetQM + result.b << endl;

    return result;
}

double CoCpnNcsContext::getRateForQM(const double actualQM, const double targetQM) {
    RateAdjustment function(this);

    return function(targetQM);
}

double CoCpnNcsContext::getQMForRate(const double actualQM, const double rate) {
    RateAdjustment function(this);

    FunctionTools::Derivative derive(function, DIFF_H);
    FunctionTools::BisectSolve bisect(function, function.limit, BISECT_ITER_LIMIT, NEWTON_EPSILON);
    FunctionTools::NewtonSolve newton(derive, bisect, NEWTON_ITER_LIMIT, NEWTON_EPSILON);

    return newton.solve(rate, actualQM);
}

double CoCpnNcsContext::getAvgFrequencyForQM(const double actualQM, const double targetQM) {
    const double targetQoC = qmToQoC(targetQM);

    return ncs()->getRateFunction()(targetQoC);
}

double CoCpnNcsContext::qocToQM(const double qoc) {
    // clamp, QoC must always be within the range [0.0,1.0]
    const double inQoC = CLAMP(qoc, 0.0, 1.0);

    // do not clamp, some QoC values might correspond to an out-of-range QM
    // this information might be required somewhere else
    const double result = (inQoC - offset) * factor;

    //EV_TRACE << "QoC -> QM: f(" << qoc << ") = " << result << endl;

    return result;
}

double CoCpnNcsContext::qmToQoC(const double qm) {
    // do not clamp, some out-of-range QM values correspond to real QoC values
    const double inQM = qm;

    // clamp, QoC must always be within the range [0.0,1.0]
    const double result = CLAMP((inQM / factor + offset), 0.0, 1.0);

    //EV_TRACE << "QM -> QoC: f^-1(" << qm << ") = " << result << endl;

    return result;
}

CoCpnNcsContext::RateAdjustment::RateAdjustment(CoCpnNcsContext * const parent)
        : parent(parent) {
    ASSERT(parent);
}

double CoCpnNcsContext::RateAdjustment::eval(const double targetQM) const {
    const double targetQoC = parent->qmToQoC(targetQM);

    double result = parent->ncs()->getRateFunction()(targetQoC);

    result *= (parent->getPayloadSize() + parent->perPacketOverhead) * 8;
    result += parent->networkOverhead * 8;

    return result;
}
