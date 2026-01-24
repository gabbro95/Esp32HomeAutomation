#include "GateDevice.h"
#include "PeerManager.h"
#include <Arduino.h>
// Creazione di GateDevice
GateDevice::GateDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_GATE, mac), peerManager(manager) {
    Gate.gateActual;
    Gate.isMoving = false;
    Gate.pending = false;
    switchOffLightTimer.setInterval(OFF_LIGHT_INTERVAL_MS);
    switchOffMovingTimer.setInterval(OFF_TIMER_GATE_MOVING_INTERVAL_MS);
    switchMovingSecurityTimer.setInterval(OFF_TIMER_GATE_INTERVAL_MS);
}
// Messaggio ricevuto dal Gate
void GateDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        if (msg.deviceId == DEV_GATE) {
            GateActualState oldState = msg.gateActual;
            Gate.pending = false;
            evaluateAutomationRules_GateChanged(oldState, Gate.gateActual);
            peerManager->mirrorStatusToUIs(msg);
        } else if (msg.deviceId == DEV_SMALL_GATE) {
            SmallGateActualState oldState = msg.smallGateActual;
            SmallGate.isOnLight = msg.stateOn;
            SmallGate.pending = false;
            evaluateAutomationRules_SmallGateChanged(oldState, SmallGate.smallGateActual);
            peerManager->mirrorStatusToUIs(msg);
        }
    }
}
// Set pending GateDevice
void GateDevice::setPending(bool pending) {
    Gate.pending = pending;
}
// Logica di automazione delle Luci al movimento Gate
void GateDevice::evaluateAutomationRules_GateChanged(GateActualState oldState, GateActualState newState) {
    // La logica si attiva solo quando lo stato cambia e il cancello inizia ad aprirsi
    if (oldState == newState) return;

    if (newState == GATE_ACTUAL_OPEN) {
        if (peerManager->getState().isNight) {
            Serial.println("[Gate] Accendo luci.");
            // Controllo Luci Esterne
            if (!peerManager->getState().isOnLight)  {
                peerManager->setState(true);
                peerManager->sendUpdateState();
            }
            // Controllo Luci Garage
            if (!getGarageState().isOnLight) {            
                peerManager->sendToggleMessage(DEV_GARAGE);
            }
        }
    }
    else if (newState == GATE_ACTUAL_CLOSED) switchOffLightTimer.reset();
}
// Logica di automazione delle Luci al movimento SmallGate
void GateDevice::evaluateAutomationRules_SmallGateChanged(SmallGateActualState oldState, SmallGateActualState newState) {
    // La logica si attiva solo quando lo stato cambia
    if (oldState == newState) return;

    if (peerManager->getState().isNight) {
        if (newState == SMALL_GATE_ACTUAL_OPEN) {
            Serial.println("[SmallGate] Accendo luci.");
            // Controllo Luci Esterne
            if (!peerManager->getState().isOnLight)  {
                peerManager->setState(true);
                peerManager->sendUpdateState(); 
            }
            // Controllo Luci Garage
            if (!getGarageState().isOnLight) peerManager->sendToggleMessage(DEV_GARAGE);
            // Controllo Luci SmallGate
            if (!getSmallGateState().isOnLight) peerManager->sendToggleMessage(DEV_SMALL_GATE);
        }
        else if (newState == SMALL_GATE_ACTUAL_CLOSED) switchOffLightTimer.reset(); 
    }
}
// Questa funzione è il loop() principale del tuo progetto
void GateDevice::loop() {
    // Gestione Stato
    if (!switchOffMovingTimer.check() && Gate.gateActual == GATE_ACTUAL_OPENING) {
        Gate.isMoving = true;
        switchOffMovingTimer.reset();
        switchMovingSecurityTimer.reset();
    } else if (Gate.gateActual == GATE_ACTUAL_OPEN) {
        if (switchOffMovingTimer.check()) switchOffMovingTimer.stop();
    } else if (!switchOffMovingTimer.check() && Gate.gateActual == GATE_ACTUAL_CLOSING) {
        switchOffMovingTimer.reset();
    } else if (!switchOffMovingTimer.check() && Gate.gateActual == GATE_ACTUAL_CLOSED) {
        Gate.isMoving = false;
        switchOffMovingTimer.stop();
        switchMovingSecurityTimer.stop();
    } else {
        if (switchOffMovingTimer.check() && switchOffMovingTimer.isExpired() && Gate.gateActual != GATE_ACTUAL_CLOSED) {
            peerManager->sendToggleMessage(this->getDeviceId());
            switchOffMovingTimer.stop();
        } else if (switchMovingSecurityTimer.check() && switchMovingSecurityTimer.isExpired()) {
            Gate.isMoving = true;
            switchOffMovingTimer.reset();
            switchMovingSecurityTimer.reset();
        }
    }

    // Timer di sicurezza
    if (switchMovingSecurityTimer.check() && switchMovingSecurityTimer.isExpired()) {
        if (!switchOffMovingTimer.check() && switchOffMovingTimer.isExpired() && Gate.isMoving) {
            if (Gate.gateActual == GATE_ACTUAL_OPEN) {
                peerManager->sendToggleMessage(this->getDeviceId());
            }
            Gate.isMoving = false;
            peerManager->sendStatusMessage(peerManager->macCentral);
        }
    }
    // Timer di spegnimento Luci
    if (switchOffLightTimer.check() && switchOffLightTimer.isExpired()) {
        Serial.println("[Gate] Timer spegnimento scaduto. Spengo luci.");
        // Controllo Luci Esterne
        if (peerManager->getState().isOnLight) {
            peerManager->setState(false);
            peerManager->sendUpdateState();
        } 
        // Controllo Luci Garage
        if (getGarageState().isOnLight) {
            peerManager->sendToggleMessage(DEV_GARAGE);
        }
    }
}