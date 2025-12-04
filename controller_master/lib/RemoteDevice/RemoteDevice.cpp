#include "RemoteDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

RemoteDevice::RemoteDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_REMOTE, mac), peerManager(manager) {}

void RemoteDevice::handleMessage(const EspNowMessage& msg) {
    DeviceType finalDest = msg.deviceId; // Destinazione finale (es. DEV_GARAGE)

    if (msg.command == CMD_PING) {
        peerManager->sendFullStateToUI(getMacAddress());
        return;
    }

    if (finalDest == DEV_CENTRAL_MASTER) {
        if (msg.command == CMD_TOGGLE)  {
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
        PeerDevice* target;
        if (finalDest == DEV_GARAGE) target = peerManager->findPeerByDevice(static_cast<DeviceType>(DEV_CENTRAL));
        else target = peerManager->findPeerByDevice(static_cast<DeviceType>(finalDest));
        
        if (!target) {
            Serial.println("[rx] remote -> target non trovato");
            return;
        }

        peerManager->clearPendingForDevice(target->getDeviceId());

        EspNowMessage toggle{};
        toggle.deviceId = finalDest;
        toggle.command = msg.command;
        toggle.sequenceNum = peerManager->getNextSequenceNum();
        peerManager->sendOrQueue(target->getMacAddress(), toggle);
        return;
    }
}