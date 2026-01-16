#include "SmallGateDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

SmallGateDevice::SmallGateDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_SMALL_GATE, mac), peerManager(manager) {
    stateSmallGate.smallGateActual;
    stateSmallGate.isOn = false;
    stateSmallGate.isCall = false;
    stateSmallGate.pending = false;
    switchOffTimer.setInterval(OFF_INTERVAL_MS);
    switchOffSecurityTimer.setInterval(OFF_TIMER_INTERVAL_MS);
}

void SmallGateDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        SmallGateActualState oldState = stateSmallGate.smallGateActual;
        stateSmallGate.smallGateActual = msg.smallGateActual;
        stateSmallGate.isOn = msg.stateOn;
        stateSmallGate.pending = false;

        if (stateSmallGate.isOn) {
            if (!switchOffSecurityTimer.check()) switchOffSecurityTimer.reset();
            else {
                if (switchOffSecurityTimer.isExpired()) {
                    EspNowMessage msg{};
                    msg.deviceId = DEV_SMALL_GATE;
                    msg.command = CMD_TOGGLE;
                    if (peerManager->sendEspNowMessage(this->getMacAddress(), msg)) {
                        Serial.println("Invio messaggio di toogle!");
                    } else {
                        Serial.println("❌ Errore nell'invio del toogle.");
                    }
                    switchOffSecurityTimer.reset();
                }
            }
        } else switchOffSecurityTimer.stop();

        peerManager->mirrorStatusToUIs(msg, getMacAddress());
        evaluateAutomationRules_SmallGateChanged(oldState, stateSmallGate.smallGateActual);
        return;
    }
    if (msg.command == CMD_CALL) {
        PeerDevice* target = peerManager->findPeerByDevice(static_cast<DeviceType>(msg.deviceId));
        if (!target) {
            Serial.println("[rx] remote -> target non trovato");
            return;
        }

        EspNowMessage call = msg;
        call.sequenceNum = peerManager->getNextSequenceNum();
        peerManager->sendOrQueue(target->getMacAddress(), call);
        return;
    }
}

void SmallGateDevice::setPending(bool pending) {
    stateSmallGate.pending = pending;
}

// Logica di automazione del cancello (aggiornata)
void SmallGateDevice::evaluateAutomationRules_SmallGateChanged(SmallGateActualState oldState, SmallGateActualState newState) {
    // La logica si attiva solo quando lo stato cambia
    if (oldState == newState) return;

    bool sendToggle = false;

    if (peerManager->getState().isNight) {
        if (newState == SMALL_GATE_ACTUAL_OPEN) sendToggle = true;
        else if (newState == SMALL_GATE_ACTUAL_CLOSED) switchOffTimer.reset(); 
    }
    // La logica del timer è stata SPOSTATA nel loop()

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

        PeerDevice* smallGate = peerManager->findPeerByDevice(DEV_SMALL_GATE);
        if (!smallGate->getSmallGateState().isOn) {            
            EspNowMessage cmdToTarget{};
            cmdToTarget.deviceId = DEV_SMALL_GATE;
            cmdToTarget.command = CMD_TOGGLE;
            cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(garage->getMacAddress(), cmdToTarget);
        }
    }
    return;
}

// Questa funzione deve essere chiamata dal loop() principale del tuo progetto
void SmallGateDevice::loop() {
    if (switchOffTimer.check() && switchOffTimer.isExpired() && getState().smallGateActual == SMALL_GATE_ACTUAL_CLOSED) {
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
            
        PeerDevice* smallGate = peerManager->findPeerByDevice(DEV_SMALL_GATE);
        if (!smallGate->getSmallGateState().isOn) {            
            EspNowMessage cmdToTarget{};
            cmdToTarget.deviceId = DEV_SMALL_GATE;
            cmdToTarget.command = CMD_TOGGLE;
            cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(garage->getMacAddress(), cmdToTarget);
        }
    }
    return;
}