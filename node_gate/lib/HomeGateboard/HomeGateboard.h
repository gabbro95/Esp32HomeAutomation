#ifndef HOME_GATE_BOARD_H
#define HOME_GATE_BOARD_H

#include <Arduino.h>

#include <WiFi.h>       
#include <esp_now.h>
#include <esp_wifi.h>

// Includiamo le tue definizioni comuni
#include "MyCommon.h"
#include "MySecrets.h"
#include "MyTimer.h"

class HomeGateboard {
public:
    HomeGateboard();
    
    // Inizializzazione hardware e software
    void begin();
    
    // Loop principale da chiamare nel loop() di Arduino
    void update();

    void handleGateAction(EspNowMessage& msg);

private: 
    const DeviceType THIS_DEVICE_ID = DEV_GATE;

    // --- CONFIGURAZIONE PIN ---
    const int RELAY_OPEN_PIN = 26;
    const int RELAY_CLOSE_PIN = 25;
    const int RELAY_FOTO = 27; 
    const int LIMIT_SWITCH_OPEN_PIN = 22;
    const int LIMIT_SWITCH_CLOSE_PIN = 32;
    const int ULTRASONIC_PIN = 23;
    const int LED_LIMIT_SWITCH_OPEN_PIN = 13;
    const int LED_LIMIT_SWITCH_CLOSE_PIN = 14;
    const int LED_SENSOR_PIN = 17;

    // Variabili stato
    GateState gate;
    bool obstacleDetected = false;
    bool controlSensor = false; // Flag per attivare la logica del sensore
    bool controlSensorState = false; // Stato del sensore per l'autochiusura
    bool gateOpen = false;
    bool gateClose = false;
    bool sendStatus = false;
    
    uint32_t sequenceNum = 0; 

    // Timer
    MyTimer heartbeatTimer; 
    MyTimer limitSwitchTimer;
    MyTimer sensorDelayTimer;
    MyTimer sensorDelayLedTimer; 
    MyTimer autoCloseTimer; 

    void triggerPin(int PIN, bool state);
    void limitSwitch();

    // Setup ESP-NOW
    // Metodi per inviare comandi 
    void sendStatusUpdate();
    void setupEspNow();
};
// Riferimento globale
extern HomeGateboard* gateboardInstance;

#endif