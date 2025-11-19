#pragma once
#include "MyCommon.h"

class PeerDevice {
public:
    PeerDevice(DeviceType dev, const uint8_t macArr[6]);
    virtual ~PeerDevice() = default;

    virtual void handleMessage(const EspNowMessage& msg) = 0;
    
    // Nuovo metodo virtuale puro per gestire lo stato pending
    virtual void setPending(bool pending) = 0;

    DeviceType getDeviceId() const { return id; }
    const uint8_t* getMacAddress() const { return mac; }
    void setOnline(bool status) { online = status; }
    bool isOnline() const { return online; }
    void updateLastSeen() { lastSeenMs = millis(); }
    unsigned long getLastSeenMs() const { return lastSeenMs; }

protected:
    DeviceType id;
    uint8_t mac[6];
    bool online = false;
    unsigned long lastSeenMs = 0;
};