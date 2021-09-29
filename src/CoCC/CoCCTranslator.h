/*
 * CoCCTranslator.h
 *
 *  Created on: Jun 4, 2019
 *      Author: markus
 */

#ifndef COCC_COCCTRANSLATOR_H_
#define COCC_COCCTRANSLATOR_H_

#include <omnetpp.h>

#include "util/FunctionTools.h"

using namespace omnetpp;

class ICoCCTranslator {
protected:
    virtual ~ICoCCTranslator() { };

public:
    class IControlObserver {
    protected:
        virtual ~IControlObserver() { };

    public:
        virtual void postControlStep(void * const context) { };
    };

    struct CoCCLinearization {
        double m; // slope
        double b; // y intercept
    };

    virtual void setControlObserver(IControlObserver * const observer, void * const context = nullptr) = 0;

    virtual double getActualQM() = 0;
    virtual double getTargetQM() = 0;
    virtual void setTargetQM(const double targetQM) = 0;

    virtual long getPayloadSize() = 0;
    virtual long getPerPacketOverhead() = 0;
    virtual void setPerPacketOverhead(const long packetOverhead) = 0;
    virtual long getNetworkOverhead() = 0;
    virtual void setNetworkOverhead(const long networkOverhead) = 0;

    virtual double getMaxRate() = 0;

    virtual CoCCLinearization getLinearizationForRate(const double actualQM, const double targetQM) = 0;
    virtual double getRateForQM(const double actualQM, const double targetQM) = 0;
    virtual double getQMForRate(const double actualQM, const double rate) = 0;
    virtual double getAvgFrequencyForQM(const double actualQM, const double targetQM) = 0;

    class RateFunction : public FunctionTools::UnaryFunction {
    public:
        RateFunction() : FunctionTools::UnaryFunction(FunctionTools::Clamp(0.0, 1.0)) { };
        virtual ~RateFunction() { };
    };
};

typedef ICoCCTranslator* ICoCCTranslatorPtr;

#endif /* COCC_COCCTRANSLATOR_H_ */
