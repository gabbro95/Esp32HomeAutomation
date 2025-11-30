#include "SmallGateDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

SmallGateDevice::SmallGateDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_SMALL_GATE, mac), peerManager(manager) {
    state.smallGateActual;
    state.isOn = false;
    state.isCall = false;
    state.pending = false;
    switchOffTimer.setInterval(OFF_INTERVAL_MS);
}

void SmallGateDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        SmallGateActualState oldState = state.smallGateActual;
        state.smallGateActual = msg.smallGateActual;
        state.isOn = msg.stateOn;
        state.isCall = msg.stateCall;
        state.pending = false;
        peerManager->mirrorStatusToUIs(msg, getMacAddress());
        evaluateAutomationRules_SmallGateChanged(oldState, state.smallGateActual);
        return;
    }
    if (msg.command == CMD_CALL) {
        PeerDevice* target;
        if (msg.deviceId == DEV_DISPLAY_RUSTICO)    target = peerManager->findPeerByDevice(static_cast<DeviceType>(DEV_CENTRAL));
        else    target = peerManager->findPeerByDevice(static_cast<DeviceType>(msg.deviceId));
        if (!target) {
            Serial.println("[rx] remote -> target non trovato");
            return;
        }

        EspNowMessage toggle = msg;
        toggle.sequenceNum = peerManager->getNextSequenceNum();
        peerManager->sendOrQueue(target->getMacAddress(), toggle);
        return;
    }
}

void SmallGateDevice::setPending(bool pending) {
    state.pending = pending;
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
        // 1. Esegui il TOGGLE (ad esempio, da SPENTO/HIGH a ACCESO/LOW)
        int oldRelayState = digitalRead(RELAY_PIN);
        int newRelayState = !oldRelayState; // Esegue il toggle sul pin
        // 2. Calcola il NUOVO stato logico da inviare alle UI (true = Accesa se newRelayState è LOW)
        bool newLightStateOn = (newRelayState == LOW); 

        if (newLightStateOn) {
            digitalWrite(RELAY_PIN, newRelayState);
            
            PeerDevice* central = peerManager->findPeerByDevice(DEV_CENTRAL);
            EspNowMessage cmdToTarget{};
            cmdToTarget.deviceId = DEV_GARAGE;
            cmdToTarget.command = CMD_TOGGLE;
            cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(central->getMacAddress(), cmdToTarget);
            
            PeerDevice* small_gate = peerManager->findPeerByDevice(DEV_SMALL_GATE);
            EspNowMessage cmdToTarget2 = cmdToTarget;
            cmdToTarget2.deviceId = DEV_SMALL_GATE;
            cmdToTarget2.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(small_gate->getMacAddress(), cmdToTarget2);
            
            // C) Invia lo stato alle UI per feedback immediato
            EspNowMessage pendingStatus{};
            pendingStatus.deviceId = DEV_CENTRAL_MASTER;
            pendingStatus.command = CMD_STATUS;
            pendingStatus.stateOn = newLightStateOn; // INVIA IL NUOVO STATO LOGICO (true)
            pendingStatus.sequenceNum = peerManager->getNextSequenceNum(); 
            
            peerManager->mirrorStatusToUIs(pendingStatus, nullptr);
        } else {
            PeerDevice* small_gate = peerManager->findPeerByDevice(DEV_SMALL_GATE);
            EspNowMessage cmdToTarget2{};
            cmdToTarget2.deviceId = DEV_SMALL_GATE;
            cmdToTarget2.command = CMD_TOGGLE;
            cmdToTarget2.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(small_gate->getMacAddress(), cmdToTarget2); 
        }
    }
    return;
}

// Questa funzione deve essere chiamata dal loop() principale del tuo progetto
void SmallGateDevice::loop() {
    if (switchOffTimer.check() && switchOffTimer.isExpired() && getState().smallGateActual == SMALL_GATE_ACTUAL_CLOSED) {
        // Il timer è scaduto!
        Serial.println("[SmallGate] Timer spegnimento scaduto. Spengo luce.");

        // 1. Esegui il TOGGLE (da ACCESO/LOW a SPENTO/HIGH)
        int oldRelayState = digitalRead(RELAY_PIN); 
        int newRelayState = !oldRelayState; // Esegue il toggle sul pin
        // 2. Calcola il NUOVO stato logico da inviare alle UI
        bool newLightStateOn = (newRelayState == LOW); // INVIA IL NUOVO STATO LOGICO (false)

        if (!newLightStateOn) {
            digitalWrite(RELAY_PIN, newRelayState); // Spegne la luce.
            // ... e invia tutti i messaggi di stato
            PeerDevice* central = peerManager->findPeerByDevice(DEV_CENTRAL);
            EspNowMessage cmdToTarget{};
            cmdToTarget.deviceId = DEV_GARAGE;
            cmdToTarget.command = CMD_TOGGLE;
            cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(central->getMacAddress(), cmdToTarget);
            
            PeerDevice* small_gate = peerManager->findPeerByDevice(DEV_SMALL_GATE);
            EspNowMessage cmdToTarget2 = cmdToTarget;
            cmdToTarget2.deviceId = DEV_SMALL_GATE;
            cmdToTarget2.sequenceNum = peerManager->getNextSequenceNum();
            peerManager->sendOrQueue(small_gate->getMacAddress(), cmdToTarget2);
            
            EspNowMessage pendingStatus{}; 
            pendingStatus.deviceId = DEV_CENTRAL_MASTER;
            pendingStatus.command = CMD_STATUS;
            pendingStatus.stateOn = newLightStateOn; // INVIA IL NUOVO STATO LOGICO (false)
            pendingStatus.sequenceNum = peerManager->getNextSequenceNum(); 
            peerManager->mirrorStatusToUIs(pendingStatus, nullptr);
    
        } else {
            if (getState().isOn) {
                PeerDevice* small_gate = peerManager->findPeerByDevice(DEV_SMALL_GATE);
                EspNowMessage cmdToTarget2{};
                cmdToTarget2.deviceId = DEV_SMALL_GATE;
                cmdToTarget2.command = CMD_TOGGLE;
                cmdToTarget2.sequenceNum = peerManager->getNextSequenceNum();
                peerManager->sendOrQueue(small_gate->getMacAddress(), cmdToTarget2);
            }
        }
    }
    return;
}