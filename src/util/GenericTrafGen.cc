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

#include "util/GenericTrafGen.h"

#include <inet/common/RawPacket.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/lifecycle/NodeOperations.h>

Define_Module(GenericTrafGen);

simsignal_t GenericTrafGen::rcvdPkSignal = registerSignal("rcvdPk");
simsignal_t GenericTrafGen::sentPkSignal = registerSignal("sentPk");

GenericTrafGen::GenericTrafGen() {

}

GenericTrafGen::~GenericTrafGen() {
    cancelAndDelete(timer);
}

void GenericTrafGen::initialize() {
    cSimpleModule::initialize();

    inGate = gate("io$i");
    outGate = gate("io$o");

    generateRaw = par("generateRaw");
    numPackets = par("numPackets");
    startTimePar = &par("startTime");
    stopTimePar = &par("stopTime");
    packetLengthPar = &par("packetLength");
    sendIntervalPar = &par("sendInterval");

    jitterStddev = par("jitterStddev");
    jitterMin = par("jitterMin");
    jitterMax = par("jitterMax");
    jitterMultiplicative = par("jitterMultiplicative");
    jitterAdditive = par("jitterAdditive");

    jitterAccumulator = (jitterMin + jitterMax) / 2;

    startTime = startTimePar->doubleValue();
    stopTime = -1; // updated at startTime

    cycle = &par("cycle");
    cycleStart = &par("cycleStart");

    numSent = 0;
    numReceived = 0;
    WATCH(numSent);
    WATCH(numReceived);

    timer = new cMessage("sendTimer");

    startApp();
}

void GenericTrafGen::startApp() {
    if (isEnabled())
        scheduleNextPacket(-1);
}

void GenericTrafGen::handleMessage(cMessage *msg) {
    if (msg == timer) {
        if (msg->getKind() != STOP) {
            sendPacket();
        }

        if (isEnabled()) {
            scheduleNextPacket(simTime());
        }
    } else {
        processPacket(PK(msg));
    }

    if (hasGUI()) {
        char buf[40];
        sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
        getDisplayString().setTagArg("t", 0, buf);
    }
}


bool GenericTrafGen::handleOperationStage(LifecycleOperation *operation, int stage, IDoneCallback *doneCallback) {
    Enter_Method_Silent();

    if (dynamic_cast<NodeStartOperation *>(operation)) {
        if ((NodeStartOperation::Stage)stage == NodeStartOperation::STAGE_APPLICATION_LAYER)
            startApp();
    }
    else if (dynamic_cast<NodeShutdownOperation *>(operation)) {
        if ((NodeShutdownOperation::Stage)stage == NodeShutdownOperation::STAGE_APPLICATION_LAYER)
            cancelNextPacket();
    }
    else if (dynamic_cast<NodeCrashOperation *>(operation)) {
        if ((NodeCrashOperation::Stage)stage == NodeCrashOperation::STAGE_CRASH)
            cancelNextPacket();
    } else {
        throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
    }

    return true;
}

void GenericTrafGen::scheduleNextPacket(simtime_t previous) {
    simtime_t next;

    if (previous == -1) { // startup
        next = simTime() <= startTime ? startTime : simTime();
        timer->setKind(START);
    } else { // regular operation
        switch (timer->getKind()) {
        case START:
            *cycleStart = startTime.dbl(); // memorize new cycle start
            stopTime = stopTimePar->doubleValue();
            timer->setKind(NEXT);
            // fall through
        case NEXT: {
            double interval = sendIntervalPar->doubleValue();
            const double noise = jitterStddev > 0 ? normal(0, jitterStddev) : 0;

            jitterAccumulator += noise;
            jitterAccumulator = std::max(jitterMin, std::min(jitterMax, jitterAccumulator));

            const double additive = jitterAdditive * jitterAccumulator;
            const double multiplicative = jitterMultiplicative * jitterAccumulator;

            if (interval > 1E-18) {
                const double freq = 1 / interval * (1 + multiplicative) + additive;

                if (freq > 0) {
                    interval = 1 / freq;
                }
            }

            next = previous + interval;
            } break;
        case STOP: // called at stopTime
            *cycle = cycle->intValue() + 1; // increment cycle counter
            startTime = startTimePar->doubleValue(); // fetch new startTime

            // re-triggering requested
            if (startTime >= stopTime) {
                next = startTime;
                stopTime = -1; // ensure the event is scheduled again
                timer->setKind(START); // causes stopTime to be re-read at next trigger
            } else {
                return; // do not schedule again
            }
            break;
        default:
            ASSERT(false);
        }


    }

    // end of burst, prepare for re-triggering
    if (stopTime >= SIMTIME_ZERO && next >= stopTime) {
        timer->setKind(STOP);

        next = stopTime;
    }

    // schedule next packet
    if (stopTime < SIMTIME_ZERO || next <= stopTime) {
        scheduleAt(next, timer);
    }
}

void GenericTrafGen::cancelNextPacket() {
    cancelEvent(timer);
}

bool GenericTrafGen::isEnabled() {
    return numPackets == -1 || numSent < numPackets;
}

void GenericTrafGen::sendPacket() {
    char msgName[32];

    sprintf(msgName, "appData-%d", numSent);

    const long packetLength = packetLengthPar->intValue();
    cPacket *payload;

    if (generateRaw) {
        payload = new RawPacket(msgName);

        static_cast<RawPacket *>(payload)->getByteArray().setDataArraySize(packetLength);
    } else {
        payload = new cPacket(msgName);
    }

    payload->setByteLength(packetLength);

    EV_INFO << "Sending packet: ";
    printPacket(payload);
    emit(sentPkSignal, payload);

    send(payload, outGate);

    numSent++;
}

void GenericTrafGen::printPacket(cPacket *msg) {
    EV_INFO << msg << endl;
    EV_INFO << "Payload length: " << msg->getByteLength() << " bytes" << endl;
}

void GenericTrafGen::processPacket(cPacket *msg) {
    emit(rcvdPkSignal, msg);
    EV_INFO << "Received packet: ";
    printPacket(msg);

    delete msg;

    numReceived++;
}

