#include "GateDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

GateDevice::GateDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_GATE, mac), peerManager(manager) {
    stateGate.gateActual;
    stateGate.isMoving = false;
    stateGate.pending = false;
    switchOffTimer.setInterval(OFF_INTERVAL_MS);
    switchOffSecurityTimer.setInterval(OFF_TIMER_INTERVAL_MS);
}

void GateDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        GateActualState oldState = stateGate.gateActual;
        stateGate.gateActual = msg.gateActual;
        stateGate.isMoving = msg.stateOn;
        stateGate.pending = false;
        peerManager->mirrorStatusToUIs(msg, getMacAddress());
        evaluateAutomationRules_GateChanged(oldState, stateGate.gateActual);
        return;
    }
}

void GateDevice::setPending(bool pending) {
    stateGate.pending = pending;
}

// Logica di automazione del cancello (aggiornata)
void GateDevice::evaluateAutomationRules_GateChanged(GateActualState oldState, GateActualState newState) {
    // La logica si attiva solo quando lo stato cambia e il cancello inizia ad aprirsi
    if (oldState == newState) return;

    bool sendToggle = false;

    if (peerManager->getState().isNight) {
        if (newState == GATE_ACTUAL_OPENING) sendToggle = true;
        else if (newState == GATE_ACTUAL_CLOSED) switchOffTimer.reset();
    }

    if (sendToggle) {
        if (!peerManager->getState().isOn)  {
            digitalWrite(RELAY_PIN, LOW);
            switchOffSecurityTimer.setInterval(OFF_TIMER_INTERVAL_MS);
            peerManager->setState(true);
            // C) Invia lo stato PENDING (value=2) a TUTTE le UI per feedback immediato
            EspNowMessage pendingStatus{}; // Usa il messaggio inviato come base
            pendingStatus.deviceId = DEV_CENTRAL_MASTER;
            pendingStatus.command = CMD_STATUS;
            pendingStatus.stateOn = peerManager->getState().isOn;
            pendingStatus.sequenceNum = peerManager->getNextSequenceNum(); 
            peerManager->mirrorStatusToUIs(pendingStatus, nullptr); 
        }

        PeerDevice* garage = peerManager->findPeerByDevice(DEV_GARAGE);
        if (!garage->getGarageState().isOn) {            
            EspNowMessage cmdToTarget{};
            cmdToTarget.deviceId = DEV_GARAGE;
            cmdToTarget.command = CMD_TOGGLE;
            cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(garage->getMacAddress(), cmdToTarget);
        }
    }
    return;
}

// Questa funzione deve essere chiamata dal loop() principale del tuo progetto
void GateDevice::loop() {
    if (switchOffTimer.check() && switchOffTimer.isExpired() && getState().gateActual == GATE_ACTUAL_CLOSED) {
        // Il timer è scaduto!
        Serial.println("[SmallGate] Timer spegnimento scaduto. Spengo luce.");
        if (peerManager->getState().isOn) {
            digitalWrite(RELAY_PIN, HIGH); // Spegne la luce.
            peerManager->setState(false);
            EspNowMessage pendingStatus{}; 
            pendingStatus.deviceId = DEV_CENTRAL_MASTER;
            pendingStatus.command = CMD_STATUS;
            pendingStatus.stateOn = peerManager->getState().isOn;
            pendingStatus.sequenceNum = peerManager->getNextSequenceNum(); 
            peerManager->mirrorStatusToUIs(pendingStatus, nullptr); 
        }
            
        PeerDevice* garage = peerManager->findPeerByDevice(DEV_GARAGE);
        if (garage->getGarageState().isOn) {
            EspNowMessage cmdToTarget{};
            cmdToTarget.deviceId = DEV_GARAGE;
            cmdToTarget.command = CMD_TOGGLE;
            cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(garage->getMacAddress(), cmdToTarget);
        }
    }
    return;
}