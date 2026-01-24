#include "DisplayDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

// Creazione di DisplayDevice
DisplayDevice::DisplayDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_DISPLAY_CASA, mac), peerManager(manager) {}

// Messaggio ricevuto dal Display
void DisplayDevice::handleMessage(const EspNowMessage& msg) {
    // Se il comando è per la centrale stessa
    if (msg.deviceId == DEV_CENTRAL_MASTER) {
        if (msg.command == CMD_PING || msg.command == CMD_STATUS) {
            if (msg.deviceId == DEV_REMOTE) peerManager->sendFullStateToUI(this->getMacAddress());
            else peerManager->sendPongMessage(this->getMacAddress());
            Serial.printf("[%u] Ricevuto PING dal Display. Invio PONG.\n", (unsigned)peerManager->findPeerByDevice(msg.deviceId)->getDeviceId()); 
        } else if (msg.command == CMD_TOGGLE)  {
            // Ricevuto comando luci centrale: mando ACK indietro
            peerManager->sendAck(this->getMacAddress());
            if (!peerManager->getState().isOnLight) {
                peerManager->setState(true);
            } else {
                peerManager->setState(false);
            }
            peerManager->sendUpdateState();
        }
    } else {
        // Il messaggio è destinato ad ALTRI dispositivi (Ponte)
        // 1. INVIO CONFERMA (ACK) AL DISPLAY
        // Dico al display: "Ho ricevuto il tuo comando e lo sto inoltrando"
        // Invio a 'this->getMacAddress()' perché 'this' è l'oggetto del Remote e del DisplayRusticoDevice
        peerManager->sendAck(this->getMacAddress());
        Serial.printf("[%u] Comando ponte ricevuto. ACK inviato al display.\n", (unsigned)peerManager->findPeerByDevice(msg.deviceId)->getDeviceId());
        // 2. INOLTRO IL MESSAGGIO
        EspNowMessage copy{};
        copy = msg;
        peerManager->replyMessage(msg);
    }
}