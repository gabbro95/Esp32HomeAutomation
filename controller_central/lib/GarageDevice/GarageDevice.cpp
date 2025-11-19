#include "GarageDevice.h"
#include "PeerManager.h"
#include <Arduino.h>

GarageDevice::GarageDevice(const uint8_t* mac, PeerManager* manager)
    : PeerDevice(DEV_GARAGE, mac), peerManager(manager) {
    state.isOn = false;
    state.pending = false;
}

void GarageDevice::handleMessage(const EspNowMessage& msg) {
    if (msg.command == CMD_STATUS) {
        state.isOn = msg.stateOn;
        state.pending = false;
        peerManager->mirrorStatusToUIs(msg, getMacAddress());
    }
}

void GarageDevice::setPending(bool pending) {
    state.pending = pending;
}