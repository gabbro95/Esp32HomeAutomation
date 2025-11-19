#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"
#include "MyTimer.h"

class PeerManager;

class GateDevice : public PeerDevice {
public:
    GateDevice(const uint8_t* mac, PeerManager* manager);
    void handleMessage(const EspNowMessage& msg) override;
    GateState getState() const { return state; }
    void setPending(bool pending);

    void loop();
    
private:
    GateState state;
    PeerManager* peerManager;
    MyTimer switchOffTimer;
    void evaluateAutomationRules_GateChanged(GateActualState oldState, GateActualState newState);
};