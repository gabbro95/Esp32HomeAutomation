#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "MyCommon.h"
#include "MySecrets.h"
#include "MyTimer.h"

//#define DEBUG

const int RELAY_LUCI_PIN = 13;
const int LED_CASA_PIN = 12;
const int LED_RUSTICO_PIN = 14;
const int RELAY_SERRATURA_PIN = 15;
const int CHIAMATA_CASA_PIN = 35;
const int CHIAMATA_RUSTICO_PIN = 34;
const int LIMIT_SWITCH_PIN = 33;
const int LIMIT_SWITCH_LIGHT_PIN = 32;

const DeviceType THIS_DEVICE_ID = DEV_SMALL_GATE;
SmallGateState smallGateState;
uint32_t sequenceNum = 0;

MyTimer limitSwitchTimer(2000);
MyTimer limitSwitchCallCasaTimer(500);
MyTimer limitSwitchCallRusticoTimer(500);
MyTimer limitSwitchLightTimer(500);
MyTimer offLightTimer(120000);
MyTimer heartBeatTimer(HEARTBEAT_INTERVAL_MS);

bool lightPush = false;


void debugPrint(const char* msg) {
    #ifdef DEBUG
        Serial.println(msg);
    #endif
}

void sendStatus() {
    EspNowMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.version = 1;
    msg.deviceId = THIS_DEVICE_ID;
    msg.command = CMD_STATUS;
    msg.smallGateActual = smallGateState.smallGateActual;
    msg.stateCall = smallGateState.isCall;
    msg.stateOn = smallGateState.isOn;
    msg.sequenceNum = ++sequenceNum;

    heartBeatTimer.reset();

#ifdef DEBUG
    Serial.printf("[GARAGE] TX: dev=%d cmd=%d val=%d seq=%lu\n",
    msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
#endif

    esp_now_send(macCentralMaster, (uint8_t*)&msg, sizeof(msg));
}

void sendCommand(DeviceType finalDest) {
    EspNowMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.version = 1;
    msg.deviceId = finalDest;
    msg.command = CMD_CALL;
    msg.sequenceNum = 0;

    heartBeatTimer.reset();

#ifdef DEBUG
    Serial.printf("[GARAGE] TX: dev=%d cmd=%d val=%d seq=%lu\n",
    msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
#endif

    esp_now_send(macCentralMaster, (uint8_t*)&msg, sizeof(msg));
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (!macEqual(mac, macCentralMaster)) {
        #ifdef DEBUG
                Serial.println("[GARAGE] RX: Ignorato (non centrale)");
        #endif
        return;
    }

    if (len != sizeof(EspNowMessage)) {
        #ifdef DEBUG
                Serial.println("[GARAGE] RX: size errata");
        #endif
        return;
    }

    EspNowMessage msg;
    memcpy(&msg, data, sizeof(msg));

    #ifdef DEBUG
        Serial.printf("[GARAGE] RX: dev=%d cmd=%d val=%d seq=%lu\n",
        msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
    #endif

    if (msg.command == CMD_STATUS) {
        sendStatus();
    } else if (msg.command == CMD_TOGGLE) {
        smallGateState.isOn = !smallGateState.isOn;
        // 1. Esegui il TOGGLE (ad esempio, da SPENTO/HIGH a ACCESO/LOW)
        int oldRelayState = digitalRead(RELAY_LUCI_PIN);
        int newRelayState = !oldRelayState; // Esegue il toggle sul pin
        digitalWrite(RELAY_LUCI_PIN, newRelayState);
        // 2. Calcola il NUOVO stato logico da inviare alle UI (true = Accesa se newRelayState è LOW)
        bool newLightStateOn = (newRelayState == LOW); 
        lightPush = newLightStateOn;
        sendStatus();
    } else if (msg.command == CMD_OPEN) {
        if (smallGateState.smallGateActual == SMALL_GATE_ACTUAL_CLOSED) {
            int state = digitalRead(RELAY_SERRATURA_PIN);
            digitalWrite(RELAY_SERRATURA_PIN, !state);
            delay(100);
            digitalWrite(RELAY_SERRATURA_PIN, state);
        }
    } else if (msg.command == CMD_CALL) {
        smallGateState.isCall = false;
        if (msg.deviceReply == DEV_DISPLAY_CASA) digitalWrite(LED_CASA_PIN, LOW);
        else if (msg.deviceReply == DEV_DISPLAY_RUSTICO) digitalWrite(LED_RUSTICO_PIN, LOW);
        sendStatus();
    }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    #ifdef DEBUG
        Serial.printf("[GARAGE] TX CB: %s\n", (status == ESP_NOW_SEND_SUCCESS) ? "OK" : "FAIL");
    #endif
}

void limitSwitch() {
    switch (smallGateState.smallGateActual)
    {
    case SMALL_GATE_ACTUAL_OPEN:
        // Gestione chiusura cancelletto
        if (digitalRead(LIMIT_SWITCH_PIN)) {
            if (limitSwitchTimer.checkAndReset()) {
                smallGateState.smallGateActual = SMALL_GATE_ACTUAL_CLOSED;
                sendStatus();
            }
        } else limitSwitchTimer.reset();
        break;
    case SMALL_GATE_ACTUAL_CLOSED:
        // Gestione apertura cancelletto
        if (!digitalRead(LIMIT_SWITCH_PIN)) {
            if (limitSwitchTimer.checkAndReset()) {
                smallGateState.smallGateActual = SMALL_GATE_ACTUAL_OPEN;
                sendStatus();
            }
        } else limitSwitchTimer.reset();
        break;
    }
    
    
    
}


void setup() {
    #ifdef DEBUG
        Serial.begin(115200);
    #endif

    pinMode(RELAY_LUCI_PIN, OUTPUT); digitalWrite(RELAY_LUCI_PIN, HIGH);
    pinMode(RELAY_SERRATURA_PIN, OUTPUT); digitalWrite(RELAY_SERRATURA_PIN, HIGH);
    pinMode(LED_CASA_PIN, OUTPUT);
    pinMode(LED_RUSTICO_PIN, OUTPUT);
    pinMode(CHIAMATA_CASA_PIN, INPUT_PULLUP);
    pinMode(CHIAMATA_RUSTICO_PIN, INPUT_PULLUP);
    pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);
    pinMode(LIMIT_SWITCH_LIGHT_PIN, INPUT_PULLUP);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        #ifdef DEBUG
            Serial.println("❌ ESP-NOW init failed");
        #endif
        return;
    }
    esp_now_set_pmk(espNowLtk);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    // Aggiungi peer centrale
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macCentralMaster, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, espNowLtk, 16);
    esp_now_add_peer(&peerInfo);

    #ifdef DEBUG
        Serial.println("[GARAGE] Pronto");
    #endif

    limitSwitchTimer.reset();
    limitSwitchCallCasaTimer.reset();
    limitSwitchCallRusticoTimer.reset();
    limitSwitchLightTimer.reset();
    offLightTimer.reset();
    heartBeatTimer.reset();
}

void loop() {
    limitSwitch();

    // Gestione chiamata casa
    if (!digitalRead(CHIAMATA_CASA_PIN)) {
        if (limitSwitchCallCasaTimer.checkAndReset()) {
            smallGateState.isCall = true;
            digitalWrite(LED_CASA_PIN, HIGH);
            sendCommand(DEV_DISPLAY_CASA);
        }
    } else limitSwitchCallCasaTimer.reset();

    // Gestione chiamata rustico
    if (!digitalRead(CHIAMATA_RUSTICO_PIN)) {
        if (limitSwitchCallRusticoTimer.checkAndReset()) {
            smallGateState.isCall = true;
            digitalWrite(LED_RUSTICO_PIN, HIGH);
            sendCommand(DEV_DISPLAY_RUSTICO);
        }
    } else limitSwitchCallRusticoTimer.reset();

    // Gestione luce cancelletto
    if (!lightPush) {
        if (!digitalRead(LIMIT_SWITCH_LIGHT_PIN)) {
            if (limitSwitchLightTimer.checkAndReset()) {
                smallGateState.isOn = true;
                lightPush = true;
                offLightTimer.reset();
                digitalWrite(RELAY_LUCI_PIN, LOW);
                sendStatus();
            }
        } else limitSwitchLightTimer.reset();
    } else {
        if (offLightTimer.check() && offLightTimer.isExpired() && !digitalRead(RELAY_LUCI_PIN)) {
            smallGateState.isOn = false;
            lightPush = false;
            digitalWrite(RELAY_LUCI_PIN, HIGH);
            sendStatus();
        }
    }

    // Gestione heartbeat
    if (heartBeatTimer.checkAndReset()) {
        sendStatus();
    }
}