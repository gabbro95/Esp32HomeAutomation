#include "DisplayRusticoDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

// Assumo che PeerDevice abbia un costruttore che accetta il deviceType e il MAC.
DisplayRusticoDevice::DisplayRusticoDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_DISPLAY_RUSTICO, mac), peerManager(manager) {}

// Helper privato per inviare l'ACK
void sendAck(const uint8_t* macDest) {
    EspNowMessage ackMsg = {};
    ackMsg.deviceId = DEV_CENTRAL_MASTER; // Chi risponde (la centrale)
    ackMsg.command = CMD_ACK;             // Il comando di conferma
    ackMsg.sequenceNum = 0; 
    
    // Usa sendEspNowMessage globale (definito in PeerManager.h)
    sendEspNowMessage(macDest, ackMsg);
}

void DisplayRusticoDevice::handleMessage(const EspNowMessage& msg) {
    
    EspNowMessage copy = msg;
    DeviceType finalDest = msg.deviceId;
    PeerDevice* target = peerManager->findPeerByDevice(static_cast<DeviceType>(DEV_CENTRAL_MASTER));
            
    // Se il comando è per la centrale stessa
    if (msg.command == CMD_PING && target->isOnline()) {
        Serial.printf("[DisplayDevice] Ricevuto PING dal Display. Invio PONG.\n");

        EspNowMessage pongMsg = {};
        pongMsg.deviceId = finalDest;
        pongMsg.command = CMD_PONG;
        pongMsg.sequenceNum = 0; 
        pongMsg.value = 0;

        if (sendEspNowMessage(this->getMacAddress(), pongMsg)) {
            Serial.println("[DisplayDevice] PONG inviato con successo.");
        } else {
            Serial.println("[DisplayDevice] ❌ Errore nell'invio del PONG.");
        }
        return;
    } else {
        // Il messaggio è destinato ad ALTRI dispositivi (Ponte)
        
        // 1. INVIO CONFERMA (ACK) AL DISPLAY
        // Dico al display: "Ho ricevuto il tuo comando e lo sto inoltrando"
        // Invio a 'this->getMacAddress()' perché 'this' è l'oggetto DisplayRusticoDevice
        sendAck(this->getMacAddress());
        Serial.println("[DisplayDevice] Comando ponte ricevuto. ACK inviato al display.");

        // 2. INOLTRO IL MESSAGGIO
        PeerDevice* target;
        if (finalDest != DEV_GARAGE) target = peerManager->findPeerByDevice(static_cast<DeviceType>(DEV_CENTRAL_MASTER));
        else target = peerManager->findPeerByDevice(static_cast<DeviceType>(finalDest));
        
        if (target) {
            peerManager->sendOrQueue(target->getMacAddress(), copy);
        }
        return;
    }
}
