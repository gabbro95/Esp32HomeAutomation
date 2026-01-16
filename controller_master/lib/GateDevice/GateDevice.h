#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"
#include "MyTimer.h"

class PeerManager;

class GateDevice : public PeerDevice {
public:
    GateDevice(const uint8_t* mac, PeerManager* manager);
    void handleMessage(const EspNowMessage& msg) override;
    GateState getState() const { return stateGate; }
    void setPending(bool pending);

    void loop();    // Questa funzione deve essere chiamata dal loop() principale del tuo progetto
    
private:
    //GateState state;  Usiamo 'stateGate' che è 'protected' in PeerDevice
        PeerManager* peerManager;
    PeerManager* peerManager;
    MyTimer switchOffTimer;
    MyTimer switchOffSecurityTimer;
    void evaluateAutomationRules_GateChanged(GateActualState oldState, GateActualState newState);
};