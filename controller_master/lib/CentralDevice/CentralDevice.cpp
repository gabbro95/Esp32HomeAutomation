#include "CentralDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

// Assumo che PeerDevice abbia un costruttore che accetta il deviceType e il MAC.
CentralDevice::CentralDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_CENTRAL, mac), peerManager(manager) {}


void CentralDevice::handleMessage(const EspNowMessage& msg) {   
    DeviceType finalDest = msg.deviceId; // Destinazione finale (es. DEV_GARAGE)

    if (finalDest == DEV_CENTRAL_MASTER && msg.command == CMD_TOGGLE)  {
        EspNowMessage pending{};

        int state = digitalRead(RELAY_PIN);
        digitalWrite(RELAY_PIN, !state);

        pending.deviceId = finalDest;
        pending.command = CMD_STATUS;
        pending.stateOn = state;
        peerManager->mirrorStatusToUIs(pending, nullptr);
        return;
    } else {
        if (msg.command == CMD_STATUS && finalDest == DEV_GARAGE) {
            peerManager->mirrorStatusToUIs(msg, this->getMacAddress());
        } else {
            if (msg.command == CMD_STATUS) {
                peerManager->mirrorStatusToUIs(msg, this->getMacAddress());
            } else {
                PeerDevice* device = peerManager->findPeerByDevice(finalDest);
                peerManager->sendOrQueue(device->getMacAddress(), msg); 
            }
        }
    }
    return;
}

