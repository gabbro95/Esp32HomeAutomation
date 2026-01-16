#include "GarageDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

GarageDevice::GarageDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_GARAGE, mac), peerManager(manager) {
    // MODIFICA: Usiamo stateGarage (ereditata) invece di state (locale)
    stateGarage.isOn = false;
    stateGarage.pending = false;
    switchOffSecurityTimer.setInterval(OFF_TIMER_INTERVAL_MS);
}

void GarageDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        stateGarage.isOn = msg.stateOn;
        stateGarage.pending = false;
        peerManager->mirrorStatusToUIs(msg, getMacAddress());
        return;
    }
}

void GarageDevice::setPending(bool pending) {
    stateGarage.pending = pending;
}

void GarageDevice::loop() {
    if (stateGarage.isOn) {
        if (!switchOffSecurityTimer.check()) switchOffSecurityTimer.reset();
        else {
            if (switchOffSecurityTimer.isExpired()) {
                EspNowMessage msg{};
                msg.deviceId = DEV_GARAGE;
                msg.command = CMD_TOGGLE;
                if (peerManager->sendEspNowMessage(this->getMacAddress(), msg)) {
                    Serial.println("Invio messaggio di toogle!");
                } else {
                    Serial.println("❌ Errore nell'invio del toogle.");
                }
                switchOffSecurityTimer.reset();
            }
        }
    } else if (switchOffSecurityTimer.check()) switchOffSecurityTimer.stop();
}