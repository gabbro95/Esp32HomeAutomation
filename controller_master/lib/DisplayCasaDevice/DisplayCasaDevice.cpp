#include "DisplayCasaDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

// Assumo che PeerDevice abbia un costruttore che accetta il deviceType e il MAC.
DisplayCasaDevice::DisplayCasaDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_DISPLAY_CASA, mac), peerManager(manager) {}


void DisplayCasaDevice::handleMessage(const EspNowMessage& msg) {
    
    EspNowMessage copy = msg;
    DeviceType finalDest = msg.deviceId;

    // Se il comando è per la centrale stessa
    if (finalDest == DEV_CENTRAL_MASTER) {
        if (msg.command == CMD_PING) {
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
        } else if (msg.command == CMD_TOGGLE)  {
            // Ricevuto comando luci centrale: mando ACK indietro
            peerManager->sendAck(this->getMacAddress());

            EspNowMessage pending{};
            if (digitalRead(RELAY_PIN)) {
                digitalWrite(RELAY_PIN, LOW);
                pending.stateOn = true;
            } else {
                digitalWrite(RELAY_PIN, HIGH);
                pending.stateOn = false;
            }
            pending.deviceId = finalDest;
            pending.command = CMD_STATUS;
            peerManager->mirrorStatusToUIs(pending, nullptr);
            return;
        }
    } else {
        // Il messaggio è destinato ad ALTRI dispositivi (Ponte)
        
        // 1. INVIO CONFERMA (ACK) AL DISPLAY
        // Dico al display: "Ho ricevuto il tuo comando e lo sto inoltrando"
        // Invio a 'this->getMacAddress()' perché 'this' è l'oggetto DisplayRusticoDevice
        peerManager->sendAck(this->getMacAddress());
        Serial.println("[DisplayDevice] Comando ponte ricevuto. ACK inviato al display.");

        // 2. INOLTRO IL MESSAGGIO
        PeerDevice* target = peerManager->findPeerByDevice(static_cast<DeviceType>(finalDest));
        if (target) {
            peerManager->sendOrQueue(target->getMacAddress(), copy);
        }
        return;
    }
}