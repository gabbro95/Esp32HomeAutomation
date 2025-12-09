#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "MyCommon.h"
#include "MySecrets.h"
#include "MyTimer.h"  

//#define DEBUG

MyTimer spegnimentoAutomatico(30000); 

const DeviceType THIS_DEVICE_ID = DEV_REMOTE;
uint32_t remoteSeqNum = 0;
GateState gate;
SmallGateState small_gate;
GarageState garage;
LightState externLight;
DeviceType currentSelectedDevice = DEV_GATE;

void debugPrint(const char* msg) {
#ifdef DEBUG
    Serial.println(msg);
#endif
}

void drawUI(); // come nel tuo codice
void sendCommand(CommandType command) {
    EspNowMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.version = 1;
    msg.deviceId = currentSelectedDevice;
    msg.command = command;
    if (msg.deviceId == DEV_GATE) msg.value = gate.gateActual;
    else if (msg.deviceId == DEV_GARAGE) msg.value = (uint16_t)garage.isOn;
    msg.sequenceNum = ++remoteSeqNum;

    #ifdef DEBUG
        Serial.printf("[REMOTE] TX: dev=%d cmd=%d val=%d seq=%lu\n",
                    msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
    #endif

    esp_now_send(macCentralMaster, (uint8_t*)&msg, sizeof(msg));
}

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
    if (!macEqual(mac, macCentralMaster)) {
        #ifdef DEBUG
                Serial.println("[REMOTE] RX: Ignorato (non centrale)");
        #endif
        return;
    }

    if (len != sizeof(EspNowMessage)) {
        #ifdef DEBUG
                Serial.println("[REMOTE] RX: size errata");
        #endif
        return;
    }

    EspNowMessage msg;
    memcpy(&msg, data, sizeof(msg));

    #ifdef DEBUG
        Serial.printf("[REMOTE] RX: dev=%d cmd=%d val=%d seq=%lu\n",
                    msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
    #endif

    if (msg.command == CMD_STATUS) {
        if (msg.deviceId == DEV_GATE) {
            gate.gateActual = msg.gateActual;
        } else if (msg.deviceId == DEV_SMALL_GATE) {
            small_gate.smallGateActual = msg.smallGateActual;
            small_gate.isOn = msg.stateOn;
        } else if (msg.deviceId == DEV_GARAGE) {
            garage.isOn = msg.stateOn;
        } else if (msg.deviceId == DEV_CENTRAL_MASTER) {
            externLight.isOn = msg.stateOn;
        }
        drawUI();
    }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
#ifdef DEBUG
    Serial.printf("[REMOTE] TX CB: %s\n", (status == ESP_NOW_SEND_SUCCESS) ? "OK" : "FAIL");
#endif
}


// Colori
#define COLOR_GREEN M5.Lcd.color565(0, 200, 0)
#define COLOR_RED   M5.Lcd.color565(200, 0, 0)
#define COLOR_YELLOW M5.Lcd.color565(255, 200, 0)
#define COLOR_BG M5.Lcd.color565(20, 20, 20)
#define COLOR_HIGHLIGHT M5.Lcd.color565(0, 100, 255)
#define COLOR_WHITE M5.Lcd.color565(255, 255, 255)

// Disegna UI
void drawUI() {
    M5.Lcd.fillScreen(COLOR_BG);
    M5.Lcd.setTextDatum(MC_DATUM);

    M5.Lcd.setFont(&fonts::DejaVu24);
    M5.Lcd.setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
    if (currentSelectedDevice == DEV_GATE) {
        M5.Lcd.setCursor(110, 60);
        M5.Lcd.print("Cancello");
    } else if (currentSelectedDevice == DEV_SMALL_GATE) {
        M5.Lcd.setCursor(90, 60);
        M5.Lcd.print("Cancelletto");
    } else if (currentSelectedDevice == DEV_GARAGE) {
        M5.Lcd.setCursor(90, 60);
        M5.Lcd.print("Luci Garage");
    } else if (currentSelectedDevice == DEV_CENTRAL_MASTER) {
        M5.Lcd.setCursor(90, 60);
        M5.Lcd.print("Luci Esterne");
    }

    M5.Lcd.setFont(&fonts::DejaVu40);
    M5.Lcd.setCursor(80, 120);

    if (currentSelectedDevice == DEV_GATE) {
        switch (gate.gateActual) {
            case GATE_ACTUAL_CLOSED:    M5.Lcd.setTextColor(COLOR_RED);    M5.Lcd.print("CHIUSO"); break;
            case GATE_ACTUAL_OPEN:      M5.Lcd.setTextColor(COLOR_GREEN);  M5.Lcd.print("APERTO"); break;
            case GATE_ACTUAL_OPENING:
            case GATE_ACTUAL_CLOSING:   M5.Lcd.setTextColor(COLOR_YELLOW); M5.Lcd.print("MOVIMENTO"); break;
            case GATE_ACTUAL_STOPPED:   M5.Lcd.setTextColor(COLOR_WHITE);  M5.Lcd.print("FERMO"); break;
            default:                    M5.Lcd.setTextColor(COLOR_WHITE);  M5.Lcd.print("SCONOSCIUTO"); break;
        }
    } else if (currentSelectedDevice == DEV_SMALL_GATE) {
        if (!small_gate.isOn) {
            M5.Lcd.setTextColor(COLOR_RED);
            M5.Lcd.print("SPENTO");
        } else {
            M5.Lcd.setTextColor(COLOR_GREEN);
            M5.Lcd.print("ACCESO");
        }
        M5.Lcd.setCursor(80, 180);
        if (small_gate.smallGateActual != SMALL_GATE_ACTUAL_OPEN) {
            M5.Lcd.setTextColor(COLOR_RED);
            M5.Lcd.print("CHIUSO");
        } else {
            M5.Lcd.setTextColor(COLOR_GREEN);
            M5.Lcd.print("APERTO");
        }
    } else if (currentSelectedDevice == DEV_GARAGE) {
        if (!garage.isOn) {
            M5.Lcd.setTextColor(COLOR_RED);
            M5.Lcd.print("SPENTE");
        } else {
            M5.Lcd.setTextColor(COLOR_GREEN);
            M5.Lcd.print("ACCESE");
        }
    } else if (currentSelectedDevice == DEV_CENTRAL_MASTER) {
        if (!externLight.isOn) {
            M5.Lcd.setTextColor(COLOR_RED);
            M5.Lcd.print("SPENTE");
        } else {
            M5.Lcd.setTextColor(COLOR_GREEN);
            M5.Lcd.print("ACCESE");
        }
    }

    M5.Lcd.setFont(&fonts::DejaVu18);
    M5.Lcd.setTextColor(COLOR_WHITE);
    M5.Lcd.setCursor(15, 220);
    if (currentSelectedDevice == DEV_SMALL_GATE) M5.Lcd.print("Cambia      Toggle      Open");
    else M5.Lcd.print("Cambia      Toggle");

    // Batteria
    M5.Lcd.setTextDatum(BL_DATUM);
    M5.Lcd.setCursor(10, 30);
    float batt = M5.Power.getBatteryVoltage() / 1000.0f;
    if (batt > 3.8)      M5.Lcd.setTextColor(COLOR_GREEN);
    else if (batt > 3.6) M5.Lcd.setTextColor(COLOR_YELLOW);
    else                 M5.Lcd.setTextColor(COLOR_RED);
    M5.Lcd.printf("Batt: %.2fV", batt);
}

/*
 * Funzione per spegnere completamente l'M5Stack
 * Utilizza il chip di gestione dell'alimentazione (PMU).
 * L'M5Stack potrà essere riacceso solo premendo il pulsante
 * di accensione/reset.
 */
void spegniM5() {
  Serial.println("Spegnimento in corso...");
  delay(100); // Piccolo ritardo per permettere l'invio del messaggio seriale
  
  // Questo è il comando che dice al PMU di spegnersi
  M5.Power.powerOff();
}


void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW init failed");
        return;
    }
    esp_now_set_pmk(espNowLtk);
    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macCentralMaster, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, espNowLtk, 16);
    esp_now_add_peer(&peerInfo);

    #ifdef DEBUG
        Serial.println("[REMOTE] Pronto");
    #endif

    drawUI();

    delay(100);
    // Invia PING (Heartbeat) a DEV_CENTRAL
    sendCommand(CMD_STATUS);

    // start timers
    spegnimentoAutomatico.reset();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) {
        switch (currentSelectedDevice)
        {
        case DEV_GATE:
            currentSelectedDevice = DEV_SMALL_GATE;
            break;
        
        case DEV_SMALL_GATE:
            currentSelectedDevice = DEV_GARAGE;
            break;
        
        case DEV_GARAGE:
            currentSelectedDevice = DEV_CENTRAL_MASTER;
            break;
        
        case DEV_CENTRAL_MASTER:
            currentSelectedDevice = DEV_GATE;
            break;
        }
        drawUI();
        sendCommand(CMD_PING);
        spegnimentoAutomatico.reset();
    }

    if (M5.BtnB.wasPressed()) {
        sendCommand(CMD_TOGGLE);
        spegnimentoAutomatico.reset();
    }

    if (M5.BtnC.wasPressed()) {
        sendCommand(CMD_OPEN);
        spegnimentoAutomatico.reset();
    }

    // Chiama la funzione di spegnimento
    if (spegnimentoAutomatico.isExpired()) spegniM5(); 
}
