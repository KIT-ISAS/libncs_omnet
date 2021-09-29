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

#include "CoCpnMockNcsImpl.h"

#include <algorithm>

Define_Module(CoCpnMockNcsImpl);

#define NEWTON_EPSILON 1E-8
#define NEWTON_ITER_LIMIT 10
#define BISECT_ITER_LIMIT 5
#define DIFF_H 1E-5
#define UPSCALE_THRESH 1.001


void CoCpnMockNcsImpl::initialize() {
}

void CoCpnMockNcsImpl::handleMessage(cMessage * const msg) {
    error("CoCpnMockNcsImpl received unexpected message");
}

void CoCpnMockNcsImpl::initializeNcs(NcsContext * const context) {
    ASSERT(context);

    this->context = context;
    parameters = context->getParameters();
    signals = context->getSignals();

    // initialize local signals

    s_unscaledPktUtility = registerSignal("mockUnscaledPktUtility");
    s_predictedPktUtility = registerSignal("mockPredictedPktUtility");
    s_predictedPktUtilityVariance = registerSignal("mockPredictedPktUtilityVariance");
    s_shortPredictedPktUtility = registerSignal("mockShortPredictedPktUtility");
    s_shortPredictedPktUtilityVariance = registerSignal("mockShortPredictedPktUtilityVariance");
    s_pktUtility = registerSignal("mockPktUtility");
    s_predictedQoC = registerSignal("mockPredictedQoC");
    s_avgQoC = registerSignal("mockAvgQoC");
    s_actQoC = registerSignal("mockActQoC");
    s_avgRate = registerSignal("mockAvgRate");
    s_actRate = registerSignal("mockActRate");
    s_rateAccumulator = registerSignal("mockRateAccumulator");
    s_ratePhaseShift = registerSignal("mockRatePhaseShift");

    // get parameters

    fillRawPackets = par("fillRawPackets").boolValue();
    mockFunction = par("mockFunction").stdstringValue();
    const double tickerDrift = par("tickerDrift").doubleValue();
    tickerInterval = par("tickerInterval").doubleValue() * tickerDrift;
    maxPktDelay = par("maxPktDelay").intValue();
    delayUtilityFactor = par("delayUtilityFactor").doubleValue();
    delayUtilityOffset = par("delayUtilityOffset").doubleValue();
    delayUtilityLimit = par("delayUtilityLimit").doubleValue();

    const double a = par("a").doubleValue();
    const double b = par("b").doubleValue();
    const double c = par("c").doubleValue();
    const double s = par("s").doubleValue();
    const double t = par("t").doubleValue();
    const double utilityEvalStep = par("utilityEvalStep").doubleValue();
    const double utilityOffset = par("utilityOffset").doubleValue();

    historySamplingSteps = par("historySamplingSteps").intValue();
    utilitySamplingSteps = par("utilitySamplingSteps").intValue();

    useRatePrediction = par("useRatePrediction").boolValue();
    ratePredictionShortWindowSteps = par("ratePredictionShortWindowSteps").intValue();
    ratePredictionLongWindowSteps = par("ratePredictionLongWindowSteps").intValue();
    ratePredictionTruncationThreshold = par("ratePredictionTruncationThreshold").doubleValue();
    ratePredictionTruncatedLongWindowSteps = par("ratePredictionTruncatedLongWindowSteps").intValue();

    const long qocRandomizationSteps = par("qocRandomizationSteps").intValue();
    const double qocRandomizationSpread = par("qocRandomizationSpread").doubleValue();
    const long rateRandomizationSteps = par("rateRandomizationSteps").intValue();
    const double rateRandomizationSpread = par("rateRandomizationSpread").doubleValue();
    rateJitter = par("rateJitter").doubleValue();

    sensorPayload = par("sensorPayload").intValue();
    controllerPayload = par("controllerPayload").intValue();

    // evaluate utility weighting function for sample points
    utilityWeights.resize(historySamplingSteps, 0.0);

    double weightSum = 0;

    // compute weight function
    for (unsigned long i = 0; i < historySamplingSteps; i++) {
        const double x = i * utilityEvalStep + utilityOffset;
        const double distort = s * x / (t + x);
        const double weight = pow(distort, a) / (b + pow(distort, c));

        weightSum += weight;
        utilityWeights[i] = weight;
    }
    EV_DEBUG << "utility weights: ";
    for (unsigned long i = 0; i < historySamplingSteps; i++) {
        utilityWeights[i] /= weightSum; // normalize such that sum of all weights equals 1

        EV_DEBUG << utilityWeights[i] << ", ";
    }
    EV_DEBUG << endl;

    // prepare state

    predictedPktUtility = 1; // initially set to 1 until prediction/filtering kicks in
    step = 0;
    pktCounter = 0;
    rateAccumulator = 0;
    ratePhaseShift = 0;
    phaseShiftTracker = 0;
    lastPhaseShiftRate = 0;
    actualQoC = 0;
    predictedQoC = 0;
    nextTargetQoC = 0;
    targetQoCChanged = false;

    auto cocpnParameters = dynamic_cast<const CoCpnNcsContext::CoCpnNcsParameters*>(parameters);

    if (cocpnParameters) {
        if (!cocpnParameters->ignoreDeadband && cocpnParameters->controllerDeadband->doubleValue() >= 0) {
            activeTargetQoC = cocpnParameters->controllerDeadband->doubleValue();
        } else {
            activeTargetQoC = 1;
        }
    }

    qocValues.resize(historySamplingSteps, 0.0);
    rateValues.resize(utilitySamplingSteps, 0.0);
    pktCount.resize(utilitySamplingSteps, 1); // initialize nonzero to simulate startup delay
    pktUtilityHistory.stats[0].resize(ratePredictionShortWindowSteps);
    pktUtilityHistory.stats[1].resize(ratePredictionLongWindowSteps);

    qocRandomizer = RandomInterpolator::createRandomInterpolator(
            qocRandomizationSteps, getRNG(0), 0, qocRandomizationSpread);
    rateRandomizer = RandomInterpolator::createRandomInterpolator(
            rateRandomizationSteps, getRNG(0), 1, rateRandomizationSpread);

    // create mock function instance

    cModuleType * const moduleType = cModuleType::get(mockFunction.c_str());
    cModule * const module = moduleType->createScheduleInit("function", this);

    function = dynamic_cast<MockFunction * const>(module);

    if (function == nullptr) {
        error("Failed to instantiate mock function implementation %s", mockFunction.c_str());
    }
}


void CoCpnMockNcsImpl::finishNcs() {
}

const simtime_t& CoCpnMockNcsImpl::getPlantPeriod() {
    return tickerInterval;
}

const simtime_t& CoCpnMockNcsImpl::getControlPeriod() {
    return tickerInterval;
}

void CoCpnMockNcsImpl::doPlantStep(const simtime_t& ncsTime, NcsContext::NcsPlantStepResult * const result) {
    // nothing to do here, the magic happens in doControlStep
    result->plantStateAdmissible = true;
}

void CoCpnMockNcsImpl::doControlStep(const simtime_t& ncsTime, NcsContext::NcsControlStepResult * const result) {
    ASSERT(dynamic_cast<CoCpnNcsContext::CoCpnNcsControlStepResult*>(result));

    auto cocpnResult = reinterpret_cast<CoCpnNcsContext::CoCpnNcsControlStepResult*>(result);

    // mocks never fail
    result->controllerStateAdmissible = true;

    //
    // update QoC
    //

    // get utility of recently received packets
    const double pktUtility = computeRecentPktUtility();

    // update long-term utility prediction used for rate prediction
    updateLongTermUtilityPrediction();

    // predict future QoC based on actualQoC and recent packet utility

    predictedQoC = getQoCForPktRate(CLAMP(pktUtility, 0.0, 1.0));

    EV_DEBUG << "predicted QoC based on observed delays and losses: " << predictedQoC << endl;

    emit(s_predictedQoC, predictedQoC);

    // compute new actualQoC based on QoC prediction and recent QoC history

    qocValues.pop_back();
    qocValues.push_front(predictedQoC);

    double avgActualQoC = 0;

    for (unsigned long i = 0; i < historySamplingSteps; i++) {
        avgActualQoC += utilityWeights[i] * qocValues[i];
    }

    const double qocRandOffset = qocRandomizer.at(step);
    actualQoC = CLAMP(avgActualQoC + qocRandOffset, 0.0, 1.0);

    EV_DEBUG << "average QoC " << avgActualQoC << " randOffset " << qocRandOffset << " actual QoC " << actualQoC << endl;

    emit(s_avgQoC, avgActualQoC);
    emit(s_actQoC, actualQoC);

    //
    // compute actual sending rate / probability
    //

    // update targetQoC if it has been changed
    // also introduce a random phase shift if target rate has been decreased significantly
    if (targetQoCChanged) {
        const double nextTargetRate = CLAMP(rawScaledFunction(nextTargetQoC), 0.0, 1.0);

        if (lastPhaseShiftRate / nextTargetRate > 1.2 && phaseShiftTracker < 0.05) {
            ratePhaseShift = uniform(-0.5, 0.5);
            phaseShiftTracker = 0.5;
            lastPhaseShiftRate = nextTargetRate;
        } else if (lastPhaseShiftRate < nextTargetRate) {
            lastPhaseShiftRate = nextTargetRate; // track rate if it increases
        }

        activeTargetQoC = nextTargetQoC;
    }

    const double targetRate = CLAMP(rawScaledFunction(activeTargetQoC), 0.0, 1.0);
    const double rateRandFactor = rateRandomizer.at(step);
    const double rate = targetRate * rateRandFactor;

    rateAccumulator += rate;
    // introduce fraction of phase shift on each step but limit influence to not outnumber the actual rate signal
    rateAccumulator += ratePhaseShift * targetRate * 0.2;
    ratePhaseShift *= 1 - targetRate * 0.2;
    phaseShiftTracker *= 1 - targetRate * 0.2;
    rateAccumulator = std::min(rateAccumulator, 1.0 + (1 - rateJitter)); // restrict carry-over-effects

    // update filter with expected rate values
    rateValues.push(targetRate);

    EV_DEBUG << "targetQoC " << activeTargetQoC << " targetRate " << targetRate / tickerInterval
            << " randFactor " << rateRandFactor << " actualRate " << rate / tickerInterval << endl;

    emit(s_avgRate, targetRate / tickerInterval);
    emit(s_actRate, rate / tickerInterval);
    emit(s_rateAccumulator, rateAccumulator);
    emit(s_ratePhaseShift, ratePhaseShift);

    //
    // Update interpolators for next step
    //

    step++;

    if (qocRandomizer.needUpdate(step)) {
        qocRandomizer.update();
    }
    if (rateRandomizer.needUpdate(step)) {
        rateRandomizer.update();
    }

    //
    // send packets
    //

    result->pkts.clear();

    const double threshold = uniform(1.0 - rateJitter, 1.0);

    EV_DEBUG << "rateAccumulator=" << rateAccumulator << " threshold=" << threshold << " " << ((rateAccumulator > threshold) ? "true, sending" : "false, not sending") << endl;

    if (rateAccumulator > threshold) {
        rateAccumulator -= 1;

        // sensor packet
        auto sensorPacket = createPkt(sensorPayload);
        sensorPacket.src = NCTXCI_SENSOR;
        sensorPacket.dst = NCTXCI_CONTROLLER;

        result->pkts.push_back(sensorPacket);

        // control packet
        auto controllerPacket = createPkt(controllerPayload);

        controllerPacket.src = NCTXCI_CONTROLLER;
        controllerPacket.dst = NCTXCI_ACTUATOR;

        result->pkts.push_back(controllerPacket);
        caHist.sent(controllerPacket.pktId, simTime());
        pktCount.push(1);
    } else {
        pktCount.push(0);
    }

    // gather other simulation results

    if (cocpnResult) {
        cocpnResult->reportedQoC = actualQoC;
    }

    //
    // collect statistical data
    //

    context->emit(signals->controlErrorSignal, actualQoC);
}

std::vector<NcsContext::NcsPkt> CoCpnMockNcsImpl::handlePacket(const simtime_t& ncsTime, NcsContext::NcsPkt& ncsPkt) {
    ncsPkt.isAck = false;

    // extract packet id
    uint64_t *idPtr = reinterpret_cast<uint64_t*>(
            ncsPkt.pkt->getByteArray().getDataPtr());

    ncsPkt.pktId = *idPtr;

    if (ncsPkt.dst == NCTXCI_ACTUATOR) {
        const simtime_t age = caHist.received(ncsPkt.pktId, simTime());
        const double utility = computeUtilityForPkt(age);

        pktUtilityHistory.push(utility);
    }

    return std::vector<NcsContext::NcsPkt>();
}

void CoCpnMockNcsImpl::updateLongTermUtilityPrediction() {
    // account for pkts which have been lost
    HistogramCollector tmpHist = caHist;

    tmpHist.prune(simTime(), tickerInterval * (maxPktDelay + 1), 0, maxPktDelay + 1);
    auto tmpHistValues = tmpHist.compute(simTime(), tickerInterval, maxPktDelay + 3, true);

    if (tmpHist.sampleCount() > 0 && *(tmpHistValues.end() - 2) > 1E-10) {
        pktUtilityHistory.push(0);
    }

    if (pktUtilityHistory.stats[1].windowSize() > pktUtilityHistory.stats[0].windowSize()) {
        // long history has enough samples to be useful
        const double shortTermMean = pktUtilityHistory.stats[0].mean();
        const double longTermMean = pktUtilityHistory.stats[1].mean();
        const double delta = shortTermMean - longTermMean;
        double longTermVariance = pktUtilityHistory.stats[1].variance();

        longTermVariance = longTermVariance < 0 ? 1E-10 : longTermVariance;

        if (delta * delta > ratePredictionTruncationThreshold * longTermVariance) {
            // short term mean has huge difference from long term mean
            // assume state change and reset long term mean
            pktUtilityHistory.truncate(ratePredictionTruncatedLongWindowSteps);

            EV_DEBUG << "resetting long term pkt utility statistics" << endl;
        }

        predictedPktUtility = pktUtilityHistory.stats[1].mean();

        EV_DEBUG << "shortTermMean=" << shortTermMean << " longTermMean=" << longTermMean << " longTermVariance=" << longTermVariance << " sqDelta=" << delta*delta << endl;

        emit(s_shortPredictedPktUtility, shortTermMean);
        emit(s_shortPredictedPktUtilityVariance, pktUtilityHistory.stats[1].variance());
        emit(s_predictedPktUtilityVariance, longTermVariance);
    }

    EV_DEBUG << "filtered packetUtility=" << predictedPktUtility << endl;
    emit(s_predictedPktUtility, predictedPktUtility);
}

double CoCpnMockNcsImpl::computeRecentPktUtility() {
    // goodput
    caHist.prune(simTime(), tickerInterval * utilitySamplingSteps, 0, utilitySamplingSteps);
    auto hist = caHist.compute(simTime(), tickerInterval, maxPktDelay + 2, true);

    EV_DEBUG << "histogram of " << caHist.sampleCount() << " pkts: ";
    for (auto val : hist) {
        EV_DEBUG << val << ", ";
    }
    EV_DEBUG << endl;

    // utility
    double pktUtility = 0;
    const double lossCompensation = hist.back() < 1.0 ? 1.0 - hist.back() : 1.0;

    for (unsigned long i = 1; i <= maxPktDelay; i++) {
        pktUtility += hist[i] * (delayUtilityFactor * i + delayUtilityOffset) * caHist.sampleCount() / lossCompensation;
    }

    EV_DEBUG << "total observed utility " << pktUtility << endl;

    // normalize for actually sent pkts
    int sentPktCount = pktCount.sum();

    if (sentPktCount > 0) {
        pktUtility /= sentPktCount;
    } else {
        pktUtility = 0;
    }

    EV_DEBUG << "average utility per packet " << pktUtility << " for " << sentPktCount << " pkts" << endl;
    emit(s_unscaledPktUtility, pktUtility);

    // account for pkts which where not sent
    double rateUtility = rateValues.mean();

    pktUtility *= rateUtility;

    EV_DEBUG << "final packetUtility after adjustment for nonsent packets " << pktUtility << endl;
    emit(s_pktUtility, pktUtility);

    return pktUtility;
}

double CoCpnMockNcsImpl::computeUtilityForPkt(const simtime_t delay) {
    const long delaySteps = std::ceil(delay / tickerInterval);

    return CLAMP(delayUtilityFactor * delaySteps + delayUtilityOffset, 0.0, delayUtilityLimit);
}

NcsContext::NcsPkt CoCpnMockNcsImpl::createPkt(const size_t len) {
    size_t bufSize = fillRawPackets ? len : sizeof(uint64_t);
    uint8_t buf[bufSize];
    uint64_t *bufAlias = reinterpret_cast<uint64_t*>(buf);
    NcsContext::NcsPkt result;

    ASSERT(len > 8);

    result.pktId = pktCounter++;
    *bufAlias = result.pktId;
    result.isAck = false;
    result.pkt = new RawPacket();
    result.pkt->setByteLength(len);
    result.pkt->setDataFromBuffer(buf, bufSize);

    return result;
}

void CoCpnMockNcsImpl::setTargetQoC(const double qoc) {
    targetQoCChanged = true;
    nextTargetQoC = CLAMP(qoc, 0.0, 1.0);
}


const ICoCCTranslator::RateFunction& CoCpnMockNcsImpl::getRateFunction() {
    return pubScaledFunction;
}

double CoCpnMockNcsImpl::getQoCForPktRate(const double rate) {
    FunctionTools::Derivative derive(rawScaledFunction, DIFF_H);
    FunctionTools::BisectSolve bisect(rawScaledFunction, rawScaledFunction.limit, BISECT_ITER_LIMIT, NEWTON_EPSILON);
    FunctionTools::NewtonSolve newton(derive, bisect, NEWTON_ITER_LIMIT, NEWTON_EPSILON);

    return newton.solve(rate, actualQoC);
}

double CoCpnMockNcsImpl::getPredictionScalingFactor() {
    const double factor = predictedPktUtility > 0 ? 1 / predictedPktUtility : 1;

    return useRatePrediction ? factor : 1.0;
}

CoCpnMockNcsImpl::ScaledMockFunction::ScaledMockFunction(CoCpnMockNcsImpl * const parent)
        : parent(parent) {
    ASSERT(parent);
}

double CoCpnMockNcsImpl::ScaledMockFunction::eval(const double targetQoC) const {
    EV_STATICCONTEXT;

    ASSERT(parent->function);

    const double actualQoC = parent->actualQoC;
    const double scale = parent->getPredictionScalingFactor();

    const double clampedActual = CLAMP(actualQoC, 0.0, 1.0);
    const double clampedTarget = CLAMP(targetQoC, 0.0, 1.0);

    double rate = parent->function->getPktRateForQoc(clampedActual, clampedTarget);

    if (scale > UPSCALE_THRESH) {
        EV_DEBUG << "rate upscaling active, actualQoC=" << actualQoC << " targetQoC=" << targetQoC << " regular rate=" << rate;

        rate = predictionScaling(scale, rate);

        EV_DEBUG << " actual rate=" << rate << endl;
    }

    return rate;
}

double CoCpnMockNcsImpl::ScaledMockFunction::predictionScaling(const double factor, const double x) const {
    const double yInterpStart = 0.95;
    const double yControlPoint = 0.99; // avoid f'(1)==0
    const double xInterpStart = yInterpStart / factor;
    const double xControlPoint = 1 / factor;

    if (x < 0) {
        return 0;
    }
    if (x > 1) {
        // try to do something reasonable for x > 1
        // continue with linear interpolation starting at x=1
        const double m = (1 - yControlPoint) / (1 - xControlPoint);

        return 1 + m * (x - 1);
    }

    if (x <= xInterpStart) {
        return factor * x;
    } else {
        // quadratic bezier interpolation for (0,0)->(a,b)->(1,1)

        // map input value from range [xInterpStart..1] into [0..1]
        const double xb = (x - xInterpStart) / (1 - xInterpStart);
        // map linear intersection with y=1 from [xInterpStart..1] into [0..1]
        // used as x-value for second interpolation point
        // maintains slope at yInterpStart, thus transition will be smooth
        double a = (xControlPoint - xInterpStart) / (1 - xInterpStart);
        // use value slightly lower than 1 as x value for second interpolation point
        // thus, at x=1 slope will be >1
        const double b = yControlPoint;

        // source: http://www.flong.com/texts/code/shapers_bez/
        // adapted from BEZMATH.PS (1993)
        // by Don Lancaster, SYNERGETICS Inc.
        // http://www.tinaja.com/text/bezmath.html
        const double epsilon = 0.00001;

        if (a == 0.5) {
            a += epsilon;
        }
        // solve t from x (an inverse operation)
        const double om2a = 1 - 2 * a;
        const double t = (sqrt(a * a + om2a * xb) - a) / om2a;
        const double y = (1 - 2 * b) * (t * t) + (2 * b) * t;

        // map y-range [0..1] into [yInterpStart..1]
        return y * (1 - yInterpStart) + yInterpStart;
    }
}

CoCpnMockNcsImpl::PublicScaledMockFunction::PublicScaledMockFunction(CoCpnMockNcsImpl::ScaledMockFunction &f, simtime_t &interval)
        : f(f), interval(interval) {
}

double CoCpnMockNcsImpl::PublicScaledMockFunction::eval(const double targetQoC) const {
    const double clamped = CLAMP(f(targetQoC), 0.0, 1.0);

    // scale up to pkt/s
    return clamped / interval;
}
