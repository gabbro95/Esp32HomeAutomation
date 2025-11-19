#pragma once
#include "MyCommon.h"
#include <vector>

class PeerDevice;
class GateDevice;
class SmallGateDevice;

enum class MsgPrio : uint8_t { Low = 0, High = 1 };

static const int RETRY_Q_SIZE = 16;
struct PendingMessage {
    EspNowMessage msg{};
    uint8_t mac[6]{};
    uint8_t attempts = 0;
    MsgPrio prio = MsgPrio::Low;
    bool active = false;
};

class PeerManager {
public:
    static PeerManager& getInstance();

    void addPeer(PeerDevice* peer);
    PeerDevice* findPeerByMac(const uint8_t* mac);
    PeerDevice* findPeerByDevice(DeviceType dev);

    void sendOrQueue(const uint8_t* mac, const EspNowMessage& msg);
    void processRetryQueue();
    void mirrorStatusToUIs(const EspNowMessage& msg, const uint8_t* excludeMac = nullptr);
    void sendFullStateToUI(const uint8_t* uiMac);

    void sendHeartbeat();
    void scanOfflinePeers();
    void clearPendingForDevice(DeviceType dev);
    uint32_t getNextSequenceNum();

    LightState getState() const { return state; }
    void setState(bool setstate);

    void loop();

private:
    PeerManager() = default;
    PeerManager(const PeerManager&) = delete;
    PeerManager& operator=(const PeerManager&) = delete;

    std::vector<PeerDevice*> peers;
    PendingMessage retryQueue[RETRY_Q_SIZE];
    int retryCount = 0;
    uint32_t sequenceNum = 0;
    
    void enqueueRetryPrio(const uint8_t* mac, const EspNowMessage& msg, MsgPrio prio);
    MsgPrio classifyPrio(const EspNowMessage& msg);

    LightState state;
};

bool sendEspNowMessage(const uint8_t* mac, const EspNowMessage& msg);