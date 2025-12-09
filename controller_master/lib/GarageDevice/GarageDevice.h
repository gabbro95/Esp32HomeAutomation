#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"

class PeerManager;

class GarageDevice : public PeerDevice {
public:
    GarageDevice(const uint8_t* mac, PeerManager* manager);
    void handleMessage(const EspNowMessage& msg) override;
    void setPending(bool pending) override; // Override del metodo base
    GarageState getState() const { return state; }

private:
    GarageState state;
    PeerManager* peerManager;
};