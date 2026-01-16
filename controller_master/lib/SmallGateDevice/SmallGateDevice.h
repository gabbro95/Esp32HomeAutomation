#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"
#include "MyTimer.h"

class PeerManager;

class SmallGateDevice : public PeerDevice {
    public:
        SmallGateDevice(const uint8_t* mac, PeerManager* manager);
        void handleMessage(const EspNowMessage& msg) override;
        void setPending(bool pending) override;                     // Override del metodo base
        SmallGateState getState() const { return stateSmallGate; }  // Non si usa più state

        void loop();

    private:
        //SmallGateState state; Usiamo 'stateSmallGate' che è 'protected' in PeerDevice
        PeerManager* peerManager;
        MyTimer switchOffTimer;
        MyTimer switchOffSecurityTimer;
        void evaluateAutomationRules_SmallGateChanged(SmallGateActualState oldState, SmallGateActualState newState);

};