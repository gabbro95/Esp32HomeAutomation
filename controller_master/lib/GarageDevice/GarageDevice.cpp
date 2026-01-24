#include "GarageDevice.h"
#include "PeerManager.h"
#include <Arduino.h>
// Creazione di GarageDevice
GarageDevice::GarageDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_GARAGE, mac), peerManager(manager) {
    // MODIFICA: Usiamo stateGarage (ereditata) invece di state (locale)
    Garage.isOnLight = false;
    Garage.pending = false;
    switchOffSecurityTimer.setInterval(OFF_TIMER_LIGHT_INTERVAL_MS);
}
// Messaggio ricevuto dal Garage
void GarageDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        Garage.isOnLight = msg.stateOn;
        Garage.pending = false;
        peerManager->mirrorStatusToUIs(msg);
    }
}
// Set pending Garage
void GarageDevice::setPending(bool pending) {
    Garage.pending = pending;
}
// Questa funzione è il loop() principale del tuo progetto
void GarageDevice::loop() {
    // Gestione Stato
    if (Garage.isOnLight) {
        if (!switchOffSecurityTimer.check()) switchOffSecurityTimer.reset();
        else {
            if (switchOffSecurityTimer.isExpired()) peerManager->sendToggleMessage(this->id);
        }
    } else if (switchOffSecurityTimer.check()) switchOffSecurityTimer.stop();
}