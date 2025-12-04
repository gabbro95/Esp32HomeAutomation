#include "CentralMasterDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

// Assumo che PeerDevice abbia un costruttore che accetta il deviceType e il MAC.
CentralMasterDevice::CentralMasterDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_CENTRAL_MASTER, mac), peerManager(manager) {}


void CentralMasterDevice::handleMessage(const EspNowMessage& msg) {   
    DeviceType finalDest = msg.deviceId; // Destinazione finale (es. DEV_GARAGE)

    PeerDevice* device;
    if (msg.command == CMD_STATUS) {
        device = peerManager->findPeerByDevice(DEV_CENTRAL_MASTER);
        peerManager->mirrorStatusToUIs(msg, getMacAddress());
    } else {
        device = peerManager->findPeerByDevice(finalDest);
        peerManager->sendOrQueue(device->getMacAddress(), msg);
    } 
    
    return;
}

