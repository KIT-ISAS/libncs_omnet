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

    startTime = startTimePar->doubleValue();
    stopTime = -1; // updated at startTime

    packetLengthPar = &par("packetLength");
    sendIntervalPar = &par("sendInterval");

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
        sendPacket();

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
        if (timer->getKind() == START) {
            stopTime = stopTimePar->doubleValue();
            timer->setKind(NEXT);
        }

        next = previous + sendIntervalPar->doubleValue();
    }

    // end of burst, prepare for re-triggering
    if (stopTime >= SIMTIME_ZERO && next >= stopTime) {
        startTime = startTimePar->doubleValue(); // fetch new startTime
        stopTime = -1; // ensure the event is scheduled again
        timer->setKind(START); // causes stopTime to be re-read at next trigger

        // computed time for next packet is outside next sequence time frame?
        // if yes, re-adjust to beginning of time frame
        if (startTime > next || (stopTime >= SIMTIME_ZERO && next >= stopTime)) {
            next = simTime() <= startTime ? startTime : simTime(); // only allow legal startTime values
        }
    }

    // schedule next packet
    if (stopTime < SIMTIME_ZERO || next < stopTime) {
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

    const long packetLength = packetLengthPar->longValue();
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

