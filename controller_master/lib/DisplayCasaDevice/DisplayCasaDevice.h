#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"

class PeerManager;

class DisplayCasaDevice : public PeerDevice {
public:
    DisplayCasaDevice(const uint8_t* mac, PeerManager* manager);
    void handleMessage(const EspNowMessage& msg) override;
    void setPending(bool pending) override {} // Implementazione vuota
    
private:
    PeerManager* peerManager;
};