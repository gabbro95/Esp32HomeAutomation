#include "GateDevice.h"
#include "PeerManager.h"
#include <Arduino.h>
#include <time.h> // Per la gestione dell'orario

GateDevice::GateDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_GATE, mac), peerManager(manager) {
    state.gateActual;
    state.isMoving = false;
    state.pending = false;
}

void GateDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        GateActualState oldState = state.gateActual;
        state.gateActual = msg.gateActual;
        state.isMoving = msg.stateOn;
        state.pending = false;
        peerManager->mirrorStatusToUIs(msg, getMacAddress());
        evaluateAutomationRules_GateChanged(oldState, state.gateActual);
        return;
    }
}

void GateDevice::setPending(bool pending) {
    state.pending = pending;
}


// Logica di automazione del cancello (aggiornata)
void GateDevice::evaluateAutomationRules_GateChanged(GateActualState oldState, GateActualState newState) {
    // La logica si attiva solo quando lo stato cambia e il cancello inizia ad aprirsi
    if (oldState == newState) return;

    bool sendToggle = false;

    if (newState == GATE_ACTUAL_OPENING) {
        if (peerManager->getState().isNight)  sendToggle = true;
    
    } else if (newState == GATE_ACTUAL_CLOSED) {
        switchOffTimer.setInterval(OFF_INTERVAL_MS);
        switchOffTimer.reset(); 
    }

    if (sendToggle) {
        // 1. Esegui il TOGGLE (ad esempio, da SPENTO/HIGH a ACCESO/LOW)
        int oldRelayState = digitalRead(RELAY_PIN);
        int newRelayState = !oldRelayState; // Esegue il toggle sul pin
        digitalWrite(RELAY_PIN, newRelayState);

        // 2. Calcola il NUOVO stato logico da inviare alle UI (true = Accesa se newRelayState è LOW)
        bool newLightStateOn = (newRelayState == LOW); 

        PeerDevice* central = peerManager->findPeerByDevice(DEV_CENTRAL);

        EspNowMessage cmdToTarget{};
        cmdToTarget.deviceId = DEV_GARAGE;
        cmdToTarget.command = CMD_TOGGLE;
        cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
        peerManager->sendOrQueue(central->getMacAddress(), cmdToTarget);
        
        // C) Invia lo stato PENDING (value=2) a TUTTE le UI per feedback immediato
        EspNowMessage pendingStatus{}; // Usa il messaggio inviato come base
        pendingStatus.deviceId = DEV_CENTRAL_MASTER;
        pendingStatus.command = CMD_STATUS;
        pendingStatus.stateOn = newLightStateOn;
        pendingStatus.sequenceNum = peerManager->getNextSequenceNum(); 
        
        // Mirrora lo stato PENDING a tutte le UI, escludendo il Display che ha inviato il comando
        peerManager->mirrorStatusToUIs(pendingStatus, nullptr); 
            
    }
}

// Questa funzione deve essere chiamata dal loop() principale del tuo progetto
void GateDevice::loop() {
    // Controlla se:
    // 1. La luce è ACCESA (presumendo HIGH = ON)
    // 2. Il cancello è CHIUSO
    // 3. Il timer (avviato da evaluate...) è SCADUTO
    if (!digitalRead(RELAY_PIN) && getState().gateActual == GATE_ACTUAL_CLOSED && switchOffTimer.check() && switchOffTimer.isExpired()) {
        // Il timer è scaduto!
        Serial.println("[SmallGate] Timer spegnimento scaduto. Spengo luce.");

        // 1. Esegui il TOGGLE (da ACCESO/LOW a SPENTO/HIGH)
        int oldRelayState = digitalRead(RELAY_PIN); 
        int newRelayState = !oldRelayState; // Esegue il toggle sul pin
        digitalWrite(RELAY_PIN, newRelayState); // Spegne la luce.

        // 2. Calcola il NUOVO stato logico da inviare alle UI
        bool newLightStateOn = (newRelayState == LOW); // INVIA IL NUOVO STATO LOGICO (false)

        // ... e invia tutti i messaggi di stato, come facevi prima
        PeerDevice* central = peerManager->findPeerByDevice(DEV_CENTRAL);
        
        EspNowMessage cmdToTarget{};
        cmdToTarget.deviceId = DEV_GARAGE;
        cmdToTarget.command = CMD_TOGGLE;
        cmdToTarget.sequenceNum = peerManager->getNextSequenceNum();
        peerManager->sendOrQueue(central->getMacAddress(), cmdToTarget);
        
        EspNowMessage pendingStatus{}; 
        pendingStatus.deviceId = DEV_CENTRAL_MASTER;
        pendingStatus.command = CMD_STATUS;
        pendingStatus.stateOn = newLightStateOn;
        pendingStatus.sequenceNum = peerManager->getNextSequenceNum(); 
        peerManager->mirrorStatusToUIs(pendingStatus, nullptr); 
    }
}