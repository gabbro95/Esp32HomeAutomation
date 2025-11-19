#include "DisplayRusticoDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

// Assumo che PeerDevice abbia un costruttore che accetta il deviceType e il MAC.
DisplayRusticoDevice::DisplayRusticoDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_DISPLAY_RUSTICO, mac), peerManager(manager) {}

void DisplayRusticoDevice::handleMessage(const EspNowMessage& msg) {
    
    EspNowMessage copy = msg;
    DeviceType finalDest = msg.deviceId;

    if (finalDest == DEV_GARAGE) {
        PeerDevice* target = peerManager->findPeerByDevice(static_cast<DeviceType>(finalDest));
        peerManager->sendOrQueue(target->getMacAddress(), copy);
    } else {
        PeerDevice* target = peerManager->findPeerByDevice(static_cast<DeviceType>(DEV_CENTRAL_MASTER));
        copy.deviceReply = getDeviceId();
        peerManager->sendOrQueue(target->getMacAddress(), copy);
    }
}
