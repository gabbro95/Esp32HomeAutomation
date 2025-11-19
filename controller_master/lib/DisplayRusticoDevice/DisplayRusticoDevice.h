#pragma once
#include "PeerDevice.h"
#include "MyCommon.h"

class PeerManager;

class DisplayRusticoDevice : public PeerDevice {
public:
    DisplayRusticoDevice(const uint8_t* mac, PeerManager* manager);
    void handleMessage(const EspNowMessage& msg) override;
    void setPending(bool pending) override {} // Implementazione vuota
    
private:
    PeerManager* peerManager;
};