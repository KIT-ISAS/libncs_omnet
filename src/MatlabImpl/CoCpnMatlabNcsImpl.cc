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

#include "CoCpnMatlabNcsImpl.h"

#include <libncs_matlab.h>

Define_Module(CoCpnMatlabNcsImpl);


void CoCpnMatlabNcsImpl::initializeNcs(NcsContext * const context) {
    ASSERT(context);

    if (!dynamic_cast<CoCpnNcsContext*>(context)) {
        error("Called from incompatible NcsContext implementation %s", context->getClassName());
    }

    s_rateDeviation = registerSignal("rateDeviation");

    MatlabNcsImpl::initializeNcs(context);
}

void CoCpnMatlabNcsImpl::doControlStep(const simtime_t& ncsTime, NcsContext::NcsControlStepResult * const result) {
    MatlabNcsImpl::doControlStep(ncsTime, result);

    lastControlStep = simTime();

    if (targetQoCChanged) {
        applyTargetQoC();
    }

    auto res = static_cast<CoCpnNcsContext::CoCpnNcsControlStepResult*>(result);

    ASSERT(res);

    res->reportedQoC = reportedQoC;
}

void CoCpnMatlabNcsImpl::setTargetQoC(const double qoc) {
    if (targetQoC != qoc) {
        targetQoC = qoc;

        if (lastControlStep != simTime()) {
            targetQoCChanged = true;
        } else {
            applyTargetQoC();
        }
    }
}

void CoCpnMatlabNcsImpl::applyTargetQoC() {
    targetQoCChanged = false;

    mwArray mw_result;
    mwArray mw_qocTarget(targetQoC);

    ncs_doHandleQocTarget(1, mw_result, ncsHandle, mw_qocTarget);

    mwArray mw_controlPeriod = mw_result.Get("samplingInterval", 1, 1);
    // should be double value, but integer is required by OMNeT++
    // we strip off the fractional part, the MATLAB part anticipates this behavior
    ASSERT(mw_controlPeriod.ClassID() == mxDOUBLE_CLASS);
    ASSERT(mw_controlPeriod.NumberOfElements() == 1);
    // strange things happen in the following line if Get(1,1) is not called
    const simtime_t newControlPeriod = SimTime(static_cast<uint64_t>(mw_controlPeriod.Get(1,1)), SIMTIME_PS);

    updateControlPeriod(newControlPeriod);

    // also retrieve the deviation factor, always positive
    mwArray mw_deviationFactor = mw_result.Get("deviationFactor", 1, 1);
    ASSERT(mw_deviationFactor.ClassID() == mxDOUBLE_CLASS);
    ASSERT(mw_deviationFactor.NumberOfElements() == 1);

    rateDeviation = 1.0 / static_cast<double>(mw_deviationFactor);

    emit(s_rateDeviation, rateDeviation);

    // semantics: actual interval = deviationFactor * interval from mapping
    // hence: actual rate = interval from mapping / deviationFactor
//    const double deviationFactorInterval = static_cast<double>(mw_deviationFactor.Get(1,1));
//    std::cout << "Deviation factor (interval):" << deviationFactorInterval << endl;
//    std::cout << "Deviation factor (rate):" << 1/ deviationFactorInterval << endl;
}

const ICoCCTranslator::RateFunction& CoCpnMatlabNcsImpl::getRateFunction() {
    return rateFunction;
}

ICoCCTranslator::CoCCLinearization CoCpnMatlabNcsImpl::getLinearizationForRate(const double actualQoC, const double targetQoC) {
    ICoCCTranslator::CoCCLinearization result;

    mwArray mw_slope;
    mwArray mw_yIntercept;
    mwArray mw_qocActual(actualQoC);
    mwArray mw_qocTarget(targetQoC);

    ncs_getLinearizationForRate(2, mw_slope, mw_yIntercept, ncsHandle, mw_qocActual, mw_qocTarget);

    result.m = static_cast<double>(mw_slope) * rateDeviation;
    result.b = static_cast<double>(mw_yIntercept) * rateDeviation;

    return result;
}

double CoCpnMatlabNcsImpl::getPktRateForQoc(const double actualQoC, const double targetQoC) {
    mwArray mw_qocActual(actualQoC);
    mwArray mw_qocTarget(targetQoC);
    mwArray mw_rate; // in packets per second

    ncs_getRateForQoc(1, mw_rate, ncsHandle, mw_qocActual, mw_qocTarget);

    double result = static_cast<double>(mw_rate) * rateDeviation;

    return result;
}

double CoCpnMatlabNcsImpl::getQoCForPktRate(const double actualQoC, const double pktRate) {
    mwArray mw_qocActual(actualQoC);
    mwArray mw_rate(pktRate / rateDeviation); // in packets per second
    mwArray mw_achivedQoC;

    ncs_getQocForRate(1, mw_achivedQoC, ncsHandle, mw_qocActual, mw_rate);

    const double result = mw_achivedQoC;

    return result;
}

CoCpnMatlabNcsImpl::RateFunction::RateFunction(CoCpnMatlabNcsImpl * const parent)
        : parent(parent) {
    ASSERT(parent);
}

double CoCpnMatlabNcsImpl::RateFunction::eval(const double targetQoC) const {
    return parent->getPktRateForQoc(parent->reportedQoC, targetQoC);
}

std::vector<const char *> CoCpnMatlabNcsImpl::getConfigFieldNames() {
    std::vector<const char *> result = MatlabNcsImpl::getConfigFieldNames();

    auto parameters = static_cast<const CoCpnNcsContext::CoCpnNcsParameters*>(this->parameters);

    testNonnegDbl(result, parameters->samplingInterval);

    if (parameters->useSampledDelayProbs) {
        result.push_back(parameters->scDelayProbs->getName());
        result.push_back(parameters->caDelayProbs->getName());
    } else {
        testNonemptyDblVect(result, parameters->scDelayProbs);
        testNonemptyDblVect(result, parameters->caDelayProbs);
    }

    testNonnegBool(result, parameters->controllerEventBased);
    testPositiveLong(result, parameters->controllerEventTrigger);
    if (!parameters->ignoreDeadband) {
        testNonnegDbl(result, parameters->controllerDeadband);
    }
    testNonnegBool(result, parameters->sensorEventBased);
    testNonnegDbl(result, parameters->sensorMeasDelta);

    testNonemptyString(result, parameters->translatorFile);

    return result;
}

void CoCpnMatlabNcsImpl::setConfigValues(mwArray &cfgStruct) {
    MatlabNcsImpl::setConfigValues(cfgStruct);

    auto parameters = static_cast<const CoCpnNcsContext::CoCpnNcsParameters*>(this->parameters);

    setNonnegDbl(cfgStruct, parameters->samplingInterval);

    if (parameters->useSampledDelayProbs) {
        NcsContext::NcsDelays delays = context->computeDelays();

        setNonemptyDblVect(cfgStruct, parameters->scDelayProbs->getName(), delays.sc);

        for (auto it = delays.ca.begin(); it != delays.ca.end(); it++) {
            printf("%.2f, ", *it);
        }
        printf("\n");
        setNonemptyDblVect(cfgStruct, parameters->caDelayProbs->getName(), delays.ca);
    } else {
        setNonemptyDblVect(cfgStruct, parameters->scDelayProbs);
        setNonemptyDblVect(cfgStruct, parameters->caDelayProbs);
    }

    setNonnegBool(cfgStruct, parameters->controllerEventBased);
    setPositiveLong(cfgStruct, parameters->controllerEventTrigger);
    if (!parameters->ignoreDeadband) {
        setNonnegDbl(cfgStruct, parameters->controllerDeadband);
    }
    setNonnegBool(cfgStruct, parameters->sensorEventBased);
    setNonnegDbl(cfgStruct, parameters->sensorMeasDelta);

    setNonemptyString(cfgStruct, parameters->translatorFile);
}
