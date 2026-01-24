#pragma once
#include "MyCommon.h"
// Gestore Device
class PeerDevice {
    public:
        PeerDevice(DeviceType dev, const uint8_t macArr[6]);        // Creazione del Device
        virtual ~PeerDevice() = default;                            // Creazione dello spazio virtuale per il Device

        virtual void handleMessage(const EspNowMessage& newMessage) = 0;  // Richiama handleMessage di Device
        
        // Nuovo metodo virtuale puro per gestire lo stato pending
        virtual void setPending(bool pending) = 0;                  // settaggio del pending

        DeviceType getDeviceId() const { return id; }               // ritorna id Device
        const uint8_t* getMacAddress() const { return mac; }        // ritorna mac Device
        void setOnline(bool status) { online = status; }            // settaggio status Device
        bool isOnline() const { return online; }                    // ritorna il valore di online Device 
        void updateLastSeen() { lastSeenMs = millis(); }            // aggiornamento di lastSeenMs
        unsigned long getLastSeenMs() const { return lastSeenMs; }  // ritorna lastSeenMs Device
        
        GarageState getGarageState() { return Garage; }             // ritorna Garage
        GateState getGateState() { return Gate; }                   // ritorna Gate
        SmallGateState getSmallGateState() { return SmallGate; }    // ritorna SmallGate

        virtual void loop() { /* Lascia vuoto */ }                  // loop del Device
        
    protected:
        DeviceType id;                      // Id device
        uint8_t mac[6];                     // Mac device
        bool online = false;                // State device
        unsigned long lastSeenMs = 0;       // Last SeenMs 

        GarageState Garage;            // Stato Garage
        GateState Gate;                // Stato Gate
        SmallGateState SmallGate;      // Stato SmallGate
};