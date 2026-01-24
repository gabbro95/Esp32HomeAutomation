#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"

class PeerManager;

class DisplayDevice : public PeerDevice {
public:
    DisplayDevice(const uint8_t* mac, PeerManager* manager);    // DisplayDevice
    void handleMessage(const EspNowMessage& _msg) override;     // messaggio ricevuto dal Display
    void setPending(bool pending) override {}                   // richiama il pending del Display
    
private:
    PeerManager* peerManager;   // Richiamo il PeerManager per leggere
};