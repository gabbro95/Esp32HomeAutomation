#include "PeerManager.h"
#include "PeerDevice.h"
#include "MyCommon.h"
#include "GarageDevice.h"
#include "GateDevice.h"
#include <Arduino.h>
#include <esp_now.h>
// Creazione del PeerManager
PeerManager& PeerManager::getInstance() {
    static PeerManager instance;
    return instance;
}
// Aggiunta di un nuovo Device
void PeerManager::addPeer(PeerDevice* device) {
    peers.push_back(device);
}
// ritorna il Device associato al mac
PeerDevice* PeerManager::findPeerByMac(const uint8_t* mac) {
    for (PeerDevice* p : peers) {
        if (macEqual(p->getMacAddress(), mac)) {
            return p;
        }
    }
    return nullptr;
}
// ritorna il Device associato al devName
PeerDevice* PeerManager::findPeerByDevice(DeviceType devName) {
    for (PeerDevice* p : peers) {
        if (p->getDeviceId() == devName) {
            return p;
        }
    }
    return nullptr;
}
// Mac centralMaster
void PeerManager::macCentralMaster() {
    uint8_t macCentralMaster[6] = { 0x64, 0xB7, 0x08, 0xC9, 0xA0, 0xB8  };
    pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, HIGH);
    pinMode(LDR_PIN, INPUT);
}
// Invio msg, se non riesce lo metto in retryQueue
void PeerManager::sendOrQueue(const uint8_t* mac, const EspNowMessage& msg) {
    if (!sendEspNowMessage(mac, msg)) {
        enqueueRetryPrio(classifyPrio(msg), msg);
        Serial.printf("[queue] added message for dev %u, prio %u\n", (unsigned)msg.deviceId, (unsigned)classifyPrio(msg));
    } else {
        Serial.printf("[tx] sent to dev %u\n", (unsigned)msg.deviceId);
    }
}
// Invio messaggi in corso
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
            // Ora msg non è più aggiornato con peerStatus
            mirrorStatusToUIs(peerStatus);
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
// Aggiornamento Display
void PeerManager::mirrorStatusToUIs(const EspNowMessage& msg) {
    for (PeerDevice* p : peers) {
        EspNowMessage copy{};
        if ( (p->getDeviceId() == DEV_REMOTE && p->isOnline()) || p->getDeviceId() == DEV_DISPLAY_CASA || p->getDeviceId() == DEV_DISPLAY_RUSTICO) {
            copy = msg;
            copy.sequenceNum = getNextSequenceNum();
            sendOrQueue(p->getMacAddress(), copy);
        }
    }
}
// Invio messaggio con lo stato di tutti i Device
void PeerManager::sendFullStateToUI(const uint8_t* uiMac) {
    // Implementazione per inviare lo stato completo

    PeerDevice* garage = findPeerByDevice(DEV_GARAGE);
    sendStatusMessage(garage->getMacAddress());

    PeerDevice* gate = findPeerByDevice(DEV_GATE);
    sendStatusMessage(garage->getMacAddress());

    PeerDevice* small_gate = findPeerByDevice(DEV_SMALL_GATE);
    sendStatusMessage(garage->getMacAddress());

    EspNowMessage msg{};
    sendUpdateState();
}
// Invio Heartbeat
void PeerManager::sendHeartbeat() {
    for (PeerDevice* p : peers) {
        if (p->isOnline()) {
            EspNowMessage msg{};
            msg.deviceId = p->getDeviceId();
            msg.command = CMD_STATUS;
            msg.statePending = p->isOnline() ? 1 : 0;
            msg.sequenceNum = getNextSequenceNum();
            sendOrQueue(p->getMacAddress(), msg);
        }
    }
}
// Ricerca Device offline
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
            mirrorStatusToUIs(peerStatus);
        }
    }
}
// Abbassa il Pending (Priorità)
void PeerManager::clearPendingForDevice(DeviceType devName) {
    PeerDevice* device = findPeerByDevice(devName);
    if (!device) return;

    // Chiama il metodo setPending dell'oggetto.
    // Il polimorfismo fa il resto: il compilatore sa che setPending
    // dell'oggetto Device corretto deve essere chiamato.
    device->setPending(false);
}
// ritorna +1 dei messaggi inviati
uint32_t PeerManager::getNextSequenceNum() {
    return ++sequenceNum;
}
// Scrive il messaggio in 
void PeerManager::enqueueRetryPrio( MsgPrio prio, const EspNowMessage& msg) {
    // Se retryCount è pieno, scrive msg su un msgPending non prioritario
    if (retryCount >= RETRY_Q_SIZE) {
        for (int i = 0; i < RETRY_Q_SIZE; ++i) {
            if (retryQueue[i].prio < prio) {
                Serial.println("[queue] Full, dropping a low prio message");
                retryQueue[i] = {};
                memcpy(retryQueue[i].mac, findPeerByDevice(msg.deviceId)->getMacAddress(), 6);
                retryQueue[i].msg = msg;
                retryQueue[i].prio = prio;
                retryQueue[i].active = true;
                return;
            }
        }
        return;
    }
    // Altrimenti aggiunge msg in coda a msgPending
    for (int i = 0; i < RETRY_Q_SIZE; ++i) {
        if (!retryQueue[i].active) {
            memcpy(retryQueue[i].mac, findPeerByDevice(msg.deviceId)->getMacAddress(), 6);
            retryQueue[i].msg = msg;
            retryQueue[i].prio = prio;
            retryQueue[i].active = true;
            retryCount++;
            return;
        }
    }
}
// Messaggio prioritario
MsgPrio PeerManager::classifyPrio(const EspNowMessage& msg) {
    if (msg.command == CMD_TOGGLE || msg.command == CMD_OPEN || msg.command == CMD_CALL || msg.command == CMD_ACK) {
        return MsgPrio::High;
    }
    return MsgPrio::Low;
}
/*
    *       Invio msg al Device associato tramite mac 
    **      ritorna ESP_OK
    ***     return 0    // se esp_now_send 
                            * è riuscito ad inviare 
                                                    * Device = msg.mac
*/
bool PeerManager::sendEspNowMessage(const uint8_t* mac, const EspNowMessage& msg) {
    esp_err_t result = esp_now_send(mac, (const uint8_t*)&msg, sizeof(msg));
    return result == ESP_OK;
}

// Helper privato per inviare l'ACK
void PeerManager::sendAck(const uint8_t* macDest) {
    EspNowMessage ackMsg = {};
    ackMsg.deviceId = DEV_CENTRAL_MASTER; // Chi risponde (la centrale)
    ackMsg.command = CMD_ACK;             // Il comando di conferma
    ackMsg.sequenceNum = 0; 
    // Usa sendEspNowMessage globale (definito in PeerManager.h)
    if (!sendEspNowMessage(macDest, ackMsg)) {
        Serial.printf("Send ACK[%u].\n", 
            (unsigned)findPeerByMac(macDest)->getDeviceId());
    } else {
        Serial.printf("Send ACK[%u].\n", 
            (unsigned)findPeerByMac(macDest)->getDeviceId());
    }
}
// Invio un messaggio di Risposta Iniziale per il Device associato a mac
void PeerManager::sendPongMessage(const uint8_t* macDevice) {
    EspNowMessage pongMsg = {};
    pongMsg.deviceId = DEV_CENTRAL_MASTER;
    pongMsg.command = CMD_PONG;
    pongMsg.sequenceNum = 0; 
    pongMsg.value = 0;
    if (!sendEspNowMessage(macDevice, pongMsg)) {
        Serial.println("[DisplayDevice] PONG inviato con successo.");
    } else {
        Serial.println("[DisplayDevice] ❌ Errore nell'invio del PONG.");
    }
}
// Invio un messaggio di Stutus 
void PeerManager::sendUpdateState() {
    EspNowMessage msg{};
    msg.deviceId = DEV_CENTRAL_MASTER;
    msg.command = CMD_STATUS;
    msg.stateOn = getState().isOnLight;
    msg.sequenceNum = getNextSequenceNum(); 
    mirrorStatusToUIs(msg); 
}
// Invio un messaggio di Status a devName
void PeerManager::sendStatusMessage(const uint8_t* uiMac) {
    EspNowMessage msg{};
    msg.deviceId = findPeerByMac(uiMac)->getDeviceId();
    msg.command = CMD_STATUS;

    if (msg.deviceId == DEV_GARAGE) {
        PeerDevice* garage = findPeerByDevice(msg.deviceId);
        msg.stateOn = garage->getGarageState().isOnLight;
    }
    else if (msg.deviceId == DEV_GATE) {
        PeerDevice* gate = findPeerByDevice(msg.deviceId);
        msg.gateActual = gate->getGateState().gateActual;
        msg.stateOn = gate->getGateState().isMoving;
    }
    else if (msg.deviceId == DEV_SMALL_GATE) {
        PeerDevice* smallGate = findPeerByDevice(msg.deviceId);
        msg.stateOn = smallGate->getSmallGateState().isOnLight;
        msg.smallGateActual = smallGate->getSmallGateState().smallGateActual;
        msg.stateCall = smallGate->getSmallGateState().isCall;
    }
    
    msg.sequenceNum = getNextSequenceNum(); 
    sendOrQueue(uiMac, msg);
}
// Invio un messaggio di Toggle a devName
void PeerManager::sendToggleMessage(DeviceType devName) {
    EspNowMessage msg{};
    msg.deviceId = devName;
    msg.command = CMD_TOGGLE;
    msg.sequenceNum = getNextSequenceNum();
    PeerDevice* device = findPeerByDevice(msg.deviceId);
    sendOrQueue(device->getMacAddress(), msg);
}
// Invio un messaggio di Apertura Cancello a msg.deviceId 
void PeerManager::sendOpenSmallGateMessage() {
    EspNowMessage msg{};
    msg.deviceId = DEV_SMALL_GATE;
    msg.command = CMD_OPEN;
    msg.sequenceNum = getNextSequenceNum();
    PeerDevice* device = findPeerByDevice(msg.deviceId);
    sendOrQueue(device->getMacAddress(), msg);
}
// Replica del messaggio ricevuto dal DisplayDevice
void PeerManager::replyMessage(const EspNowMessage& reply) {
    PeerDevice* device = findPeerByDevice(reply.deviceId);
    if (!device) return;
    EspNowMessage msg{};
    msg = reply;
    sendOrQueue(device->getMacAddress(), msg);
}
// Settaggio Timer PeerManager
void PeerManager::setTimer() {
    switchOffSecurityTimer.setInterval(OFF_TIMER_LIGHT_INTERVAL_MS);
    debounceLDRTimer.setInterval(DEBOUNCE_LDR_MS);
    heartbeatTimer.setInterval(HEARTBEAT_INTERVAL_MS);
    retryTimer.setInterval(RETRY_INTERVAL_MS);
}
// Settaggio Luci PeerManager
void PeerManager::setState(bool new_state) {
    state.isOnLight = new_state;
    if (!state.isOnLight) digitalWrite(RELAY_PIN, HIGH);
    else digitalWrite(RELAY_PIN, LOW);
}
// Loop interni condivisi con il loop principale
void PeerManager::loop() {
    // Gestione Stato Luci Esterne
    if (state.isOnLight) {
        if (!switchOffSecurityTimer.check()) switchOffSecurityTimer.reset();
        else {
            if (switchOffSecurityTimer.isExpired()) {
                digitalWrite(RELAY_PIN, HIGH);
                state.isOnLight = false;
                sendUpdateState();
            }
        }
    } else if (switchOffSecurityTimer.check()) switchOffSecurityTimer.stop();



    // Itera su tutti i dispositivi registrati e chiama il loro loop()
    // NB: Questo presume che i tuoi dispositivi siano in un array/vettore
    // chiamato "peers" e il loro numero sia "peerCount".
    // Adatta questo codice alla tua struttura dati reale.
    for (PeerDevice* p : peers) { // o qualsiasi sia il tuo modo di ciclare
        if (p->getDeviceId() == DEV_GATE || p->getDeviceId() == DEV_SMALL_GATE || p->getDeviceId() == DEV_GARAGE) {
            p->loop();
        }
    }

    scanOfflinePeers();
    if (heartbeatTimer.checkAndReset()) sendHeartbeat();
    if (retryTimer.checkAndReset()) processRetryQueue();
}