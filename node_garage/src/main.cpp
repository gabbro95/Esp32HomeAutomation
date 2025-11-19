#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "MyCommon.h"
#include "MySecrets.h"

//#define DEBUG

const int RELAY_LUCI_PIN = 13;

const DeviceType THIS_DEVICE_ID = DEV_GARAGE;
GarageState garageState;
uint32_t sequenceNum = 0;

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
    msg.stateOn = garageState.isOn;
    msg.sequenceNum = ++sequenceNum;

#ifdef DEBUG
    Serial.printf("[GARAGE] TX: dev=%d cmd=%d val=%d seq=%lu\n",
    msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
#endif

    esp_now_send(macCentral, (uint8_t*)&msg, sizeof(msg));
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (!macEqual(mac, macCentral)) {
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

    if (msg.command == CMD_TOGGLE) {
        garageState.isOn = !garageState.isOn;
        int state = digitalRead(RELAY_LUCI_PIN);
        digitalWrite(RELAY_LUCI_PIN, !state);
        sendStatus();
    } else if (msg.command == CMD_STATUS) {
        sendStatus();
    }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    #ifdef DEBUG
        Serial.printf("[GARAGE] TX CB: %s\n", (status == ESP_NOW_SEND_SUCCESS) ? "OK" : "FAIL");
    #endif
}

void setup() {
    #ifdef DEBUG
        Serial.begin(115200);
    #endif

    pinMode(RELAY_LUCI_PIN, OUTPUT); digitalWrite(RELAY_LUCI_PIN, HIGH);

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
    memcpy(peerInfo.peer_addr, macCentral, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, espNowLtk, 16);
    esp_now_add_peer(&peerInfo);

    #ifdef DEBUG
        Serial.println("[GARAGE] Pronto");
    #endif

    sendStatus();
}

void loop() {
    // Gestione heartbeat
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = millis();
        sendStatus();
    }
}
