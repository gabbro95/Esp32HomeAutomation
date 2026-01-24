#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "MySecrets.h"
#include "MyTimer.h"
#include "PeerManager.h"
#include "GarageDevice.h"
#include "GateDevice.h"
#include "DisplayDevice.h"

static const char* DEVICE_NAME = "Master";

PeerManager& peerManager = PeerManager::getInstance();

void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
    if (status == ESP_OK) {
        Serial.printf("[tx_cb] success to ");
    } else {
        Serial.printf("[tx_cb] fail to ");
    }
    logMAC(mac);
    Serial.println();
}

void onDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
    if (len != sizeof(EspNowMessage)) return;

    EspNowMessage newMessage;
    memcpy(&newMessage, incomingData, sizeof(newMessage));

    PeerDevice* sender = peerManager.findPeerByMac(mac);
    if (!sender) {
        Serial.print("[rx] mittente sconosciuto ");
        logMAC(mac);
        Serial.println();
        return;
    }
    
    sender->setOnline(true);
    sender->updateLastSeen();
    sender->handleMessage(newMessage);
}

void setupEspNow() {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    
    // Impostiamo il canale Wi-Fi
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_set_pmk(espNowLtk); // Imposta la Primary Master Key (PMK)
    
    // Registriamo i callback per l'invio e la ricezione
    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    // Configurazione e aggiunta di ogni peer
    esp_now_peer_info_t peerInfo = {};
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, espNowLtk, 16);
      
    // Aggiungi il garage
    memcpy(peerInfo.peer_addr, macGarage, 6);
    esp_now_add_peer(&peerInfo);

    // Aggiungi il cancello
    memcpy(peerInfo.peer_addr, macGate, 6);
    esp_now_add_peer(&peerInfo);
    
    // Aggiungi il cancelletto
    memcpy(peerInfo.peer_addr, macSmallGate, 6);
    esp_now_add_peer(&peerInfo);
    
    // Aggiungi il telecomando
    memcpy(peerInfo.peer_addr, macRemote, 6);
    esp_now_add_peer(&peerInfo);

    // Aggiungi il display casa
    memcpy(peerInfo.peer_addr, macDisplayCasa, 6);
    esp_now_add_peer(&peerInfo);

    // Aggiungi il display rustico
    memcpy(peerInfo.peer_addr, macDisplayRustico, 6);
    esp_now_add_peer(&peerInfo);
}

void setup() {
    Serial.begin(115200);

    delay(200);
    Serial.println("[boot] Centrale avvio...");

    setupEspNow();

    peerManager.addPeer(new GarageDevice(macGarage, &peerManager));
    peerManager.addPeer(new GateDevice(macGate, &peerManager));
    peerManager.addPeer(new GateDevice(macSmallGate, &peerManager));
    peerManager.addPeer(new DisplayDevice(macRemote, &peerManager));
    peerManager.addPeer(new DisplayDevice(macDisplayCasa, &peerManager));
    peerManager.addPeer(new DisplayDevice(macDisplayRustico, &peerManager));

    peerManager.macCentralMaster();
    peerManager.setTimer();

    Serial.println("[boot] Centrale pronta");
}

void loop() {
    peerManager.loop();
}