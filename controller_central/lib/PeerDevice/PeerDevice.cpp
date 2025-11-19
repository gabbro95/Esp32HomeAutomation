#include "PeerDevice.h"

PeerDevice::PeerDevice(DeviceType dev, const uint8_t macArr[6])
    : id(dev), online(false), lastSeenMs(0) {
    memcpy(mac, macArr, 6);
}