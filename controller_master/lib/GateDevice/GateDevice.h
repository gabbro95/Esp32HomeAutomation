#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"
#include "MyTimer.h"

class PeerManager;
// GateDevice
class GateDevice : public PeerDevice {
    public:
        GateDevice(const uint8_t* mac, PeerManager* manager);           // GateDevice    
        
        void handleMessage(const EspNowMessage& newMessage) override;   // messaggio ricevuto dal Gate
        void setPending(bool pending);                                  // richiama il pending del Gate

        GateState getState() const { return Gate; }                     // richiama il Gate

        // Loop Gate
        void loop();    // Questa funzione deve essere chiamata dal loop() principale del tuo progetto
        
    private:
        //GateState state;  Usiamo 'stateGate' che è 'protected' in PeerDevice (Shadowing)
        
        // ritorna PeerManager
        PeerManager* peerManager;

        // Timer
        MyTimer switchOffLightTimer;        // Timer per lo spegnimento Luci 
        MyTimer switchOffMovingTimer;       // Timer del movimento
        MyTimer switchMovingSecurityTimer;  // Timer di sicurezza del movimento

        // Gestione delle Luci se è notte all'apertura del cancello
        void evaluateAutomationRules_GateChanged(GateActualState oldState, GateActualState newState);
        // Gestione delle Luci se è notte
        void evaluateAutomationRules_SmallGateChanged(SmallGateActualState oldState, SmallGateActualState newState);
};