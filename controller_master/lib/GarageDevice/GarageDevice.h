#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"
#include "MyTimer.h"

class PeerManager;

class GarageDevice : public PeerDevice {
    public:
        GarageDevice(const uint8_t* mac, PeerManager* manager);
        void handleMessage(const EspNowMessage& msg) override;
    void setPending(bool pending) override;                     // Override del metodo base
        GarageState getState() const { return stateGarage; }    // Non si usa più state

        void loop();

    private:
        //GarageState state;    Usiamo 'stateGarage' che è 'protected' in PeerDevice
        PeerManager* peerManager;
        MyTimer switchOffSecurityTimer;
};