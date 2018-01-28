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

#ifndef UTIL_CRANDOMIZEDCHANNEL_H_
#define UTIL_CRANDOMIZEDCHANNEL_H_

#include <omnetpp/cchannel.h>
#include <omnetpp/csimulation.h>

namespace omnetpp {

class SIM_API cRandomizedChannel : public cIdealChannel {

  public:

    explicit cRandomizedChannel(const char *name=nullptr);

    virtual ~cRandomizedChannel();

  protected:

    virtual void initialize() override;

    virtual void handleParameterChange(const char *parname) override;

  public:

    virtual void processMessage(cMessage *msg, simtime_t t, result_t& result) override;

    virtual bool isTransmissionChannel() const override { return true; }

    /**
     * For transmission channels: Returns the nominal data rate of the channel.
     * The number returned from this method should be treated as informative;
     * there is no strict requirement that the channel calculates packet
     * duration by dividing the packet length by the nominal data rate.
     * For example, specialized channels may add the length of a lead-in
     * signal to the duration.
     */
    virtual double getNominalDatarate() const override { return 1e18; } // 1ebps should be sufficiently fast

    /**
     * For transmission channels: Calculates the transmission duration
     * of the message with the current channel configuration (datarate, etc);
     * it does not check or modify channel state. For non-transmission channels
     * this method returns zero.
     *
     * This method is useful for transmitter modules that need to determine
     * the transmission time of a packet without actually sending the packet.
     *
     * Caveats: this method is "best-effort" -- there is no guarantee that
     * transmission time when the packet is actually sent will be the same as
     * the value returned by this method. The difference may be caused by
     * changed channel parameters (i.e. "datarate" being overwritten), or by
     * a non-time-invariant transmission algorithm.
     */
    virtual simtime_t calculateDuration(cMessage *msg) const;

    /**
     * For transmission channels: Returns the simulation time
     * the sender gate will finish transmitting. If the gate is not
     * currently transmitting, the result is unspecified but less or equal
     * the current simulation time.
     */
    virtual simtime_t getTransmissionFinishTime() const;

    /**
     * For transmission channels: Returns whether the sender gate
     * is currently transmitting, ie. whether getTransmissionFinishTime()
     * is greater than the current simulation time.
     */
    virtual bool isBusy() const;

    /**
     * For transmission channels: Forcibly overwrites the finish time of the
     * current transmission in the channel (see getTransmissionFinishTime()).
     *
     * This method is a crude device that allows for implementing aborting
     * transmissions; it is not needed for normal packet transmissions.
     * Calling this method with the current simulation time will allow
     * you to immediately send another packet on the channel without the
     * channel reporting error due to its being busy.
     *
     * Note that this call does NOT affect the delivery of the packet being
     * transmitted: the packet object is delivered to the target module
     * at the time it would without the call to this method. The sender
     * needs to inform the target module in some other way that the
     * transmission was aborted and the packet should be treated accordingly
     * (i.e. discarded as incomplete); for example by sending an out-of-band
     * cMessage that the receiver has to understand.
     */
    virtual void forceTransmissionFinishTime(simtime_t t);

  private:

    void checkState() const { if (!parametersFinalized()) throw cRuntimeError(this, E_PARAMSNOTREADY); }

    void rereadPars();

  private:

    struct PdfInterval {
        simtime_t value;
        double probability;
    };

    simsignal_t messageSentSignal;
    simsignal_t messageDiscardedSignal;

    bool disabled;
    const char * pdf; // probability density function, formatted as "value_1:probability_1 value_2:probability_2", negative values represent packet drops
    std::vector<PdfInterval> pdfIntervals; // parsed probability density function
    std::vector<PdfInterval> cdfIntervals; // cumulated density function, computed from pdf
};

}

#endif /* UTIL_CRANDOMIZEDCHANNEL_H_ */
