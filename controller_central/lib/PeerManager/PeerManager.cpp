#include "PeerManager.h"
#include "PeerDevice.h"
#include "MyCommon.h"
#include "GarageDevice.h"
#include <Arduino.h>
#include <esp_now.h>

PeerManager& PeerManager::getInstance() {
    static PeerManager instance;
    return instance;
}

void PeerManager::addPeer(PeerDevice* peer) {
    peers.push_back(peer);
}

PeerDevice* PeerManager::findPeerByMac(const uint8_t* mac) {
    for (PeerDevice* p : peers) {
        if (macEqual(p->getMacAddress(), mac)) {
            return p;
        }
    }
    return nullptr;
}

PeerDevice* PeerManager::findPeerByDevice(DeviceType dev) {
    for (PeerDevice* p : peers) {
        if (p->getDeviceId() == dev) {
            return p;
        }
    }
    return nullptr;
}

void PeerManager::sendOrQueue(const uint8_t* mac, const EspNowMessage& msg) {
    if (!sendEspNowMessage(mac, msg)) {
        enqueueRetryPrio(mac, msg, classifyPrio(msg));
        Serial.printf("[queue] added message for dev %u, prio %u\n", (unsigned)msg.deviceId, (unsigned)classifyPrio(msg));
    } else {
        Serial.printf("[tx] sent to dev %u\n", (unsigned)msg.deviceId);
    }
}

void PeerManager::processRetryQueue() {
    for (int i = 0; i < RETRY_Q_SIZE; ++i) {
        if (!retryQueue[i].active) continue;
        PendingMessage &slot = retryQueue[i];
        if (slot.attempts >= RETRY_MAX_ATTEMPTS) {
            DeviceType targetDev = slot.msg.deviceId;
            PeerDevice* p = findPeerByDevice(targetDev);
            if (p) {
                p->setOnline(false);
            }
            clearPendingForDevice(targetDev);
            EspNowMessage peerStatus{};
            peerStatus.deviceId = targetDev;
            peerStatus.command = CMD_STATUS;
            peerStatus.statePending = 0;
            peerStatus.sequenceNum = getNextSequenceNum();
            mirrorStatusToUIs(peerStatus, nullptr);
            slot.active = false;
            retryCount--;
            continue;
        }
        if (!sendEspNowMessage(slot.mac, slot.msg)) {
            slot.attempts++;
            Serial.printf("[retry] fail attempt %u for dev %u\n", (unsigned)slot.attempts, (unsigned)slot.msg.deviceId);
        } else {
            Serial.printf("[retry] success for dev %u\n", (unsigned)slot.msg.deviceId);
            slot.active = false;
            retryCount--;
        }
    }
}

void PeerManager::mirrorStatusToUIs(const EspNowMessage& msg, const uint8_t* excludeMac) {
    for (PeerDevice* p : peers) {
        if (p->getDeviceId() == DEV_REMOTE || p->getDeviceId() == DEV_DISPLAY_RUSTICO || p->getDeviceId() == DEV_CENTRAL_MASTER) {
            if (excludeMac && macEqual(p->getMacAddress(), excludeMac)) continue;
            EspNowMessage copy = msg;
            copy.sequenceNum = getNextSequenceNum();
            sendOrQueue(p->getMacAddress(), copy);
        }
    }
}

void PeerManager::sendFullStateToUI(const uint8_t* uiMac) {
    // Implementazione per inviare lo stato completo
    PeerDevice* garage = findPeerByDevice(DEV_GARAGE);
    if (garage) {
        GarageDevice* gd = static_cast<GarageDevice*>(garage);
        EspNowMessage msg{};
        msg.deviceId = gd->getDeviceId();
        msg.command = CMD_STATUS;
        msg.stateOn = gd->getState().isOn;
        msg.sequenceNum = getNextSequenceNum();
        sendOrQueue(uiMac, msg);
    }
}

void PeerManager::sendHeartbeat() {
    for (PeerDevice* p : peers) {
        EspNowMessage hb{};
        hb.deviceId = p->getDeviceId();
        hb.command = CMD_STATUS;
        hb.statePending = p->isOnline() ? 1 : 0;
        hb.sequenceNum = getNextSequenceNum();
        sendOrQueue(p->getMacAddress(), hb);
    }
}

void PeerManager::scanOfflinePeers() {
    unsigned long now = millis();
    for (PeerDevice* p : peers) {
        if (p->isOnline() && (now - p->getLastSeenMs() > OFFLINE_TIMEOUT_MS)) {
            Serial.printf("[scan] Peer %u offline, last seen %lu ms ago\n", (unsigned)p->getDeviceId(), now - p->getLastSeenMs());
            p->setOnline(false);
            clearPendingForDevice(p->getDeviceId());
            EspNowMessage peerStatus{};
            peerStatus.deviceId = p->getDeviceId();
            peerStatus.command = CMD_STATUS;
            peerStatus.statePending = 0;
            peerStatus.sequenceNum = getNextSequenceNum();
            mirrorStatusToUIs(peerStatus, nullptr);
        }
    }
}

void PeerManager::clearPendingForDevice(DeviceType dev) {
    PeerDevice* target = findPeerByDevice(dev);
    if (!target) return;

    // Chiama il metodo setPending dell'oggetto.
    // Il polimorfismo fa il resto: il compilatore sa che setPending
    // dell'oggetto corretto (Garage o Gate) deve essere chiamato.
    target->setPending(false);
}

uint32_t PeerManager::getNextSequenceNum() {
    return ++sequenceNum;
}

void PeerManager::enqueueRetryPrio(const uint8_t* mac, const EspNowMessage& msg, MsgPrio prio) {
    if (retryCount >= RETRY_Q_SIZE) {
        for (int i = 0; i < RETRY_Q_SIZE; ++i) {
            if (retryQueue[i].prio < prio) {
                Serial.println("[queue] Full, dropping a low prio message");
                retryQueue[i] = {};
                memcpy(retryQueue[i].mac, mac, 6);
                retryQueue[i].msg = msg;
                retryQueue[i].prio = prio;
                retryQueue[i].active = true;
                return;
            }
        }
        return;
    }
    for (int i = 0; i < RETRY_Q_SIZE; ++i) {
        if (!retryQueue[i].active) {
            memcpy(retryQueue[i].mac, mac, 6);
            retryQueue[i].msg = msg;
            retryQueue[i].prio = prio;
            retryQueue[i].active = true;
            retryCount++;
            return;
        }
    }
}

MsgPrio PeerManager::classifyPrio(const EspNowMessage& msg) {
    if (msg.command == CMD_TOGGLE) {
        return MsgPrio::High;
    }
    return MsgPrio::Low;
}

bool sendEspNowMessage(const uint8_t* mac, const EspNowMessage& msg) {
    esp_err_t result = esp_now_send(mac, (const uint8_t*)&msg, sizeof(msg));
    return result == ESP_OK;
}