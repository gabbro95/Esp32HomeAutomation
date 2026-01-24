#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"
#include "MyTimer.h"

class PeerManager;
// GarageDevice
class GarageDevice : public PeerDevice {
    public:
        GarageDevice(const uint8_t* mac, PeerManager* manager);     // GarageDevice
        
        // ESP-NOW
        void handleMessage(const EspNowMessage& msg) override;      // Messaggio ricevuto dal Garage
        void setPending(bool pending) override;                     // richiama il pending del Garage
        
        // Stato device
        GarageState getState() const { return Garage; }             // ritorna il Garage
        
        // Loop Gate
        void loop();

    private:
        //GarageState state;    Usiamo 'stateGarage' che è 'protected' in PeerDevice (Shadowing)    
        PeerManager* peerManager;       // Accede a PeerManager
        MyTimer switchOffSecurityTimer; // Timer di sicurezza se le luci sono accese le spengo
};