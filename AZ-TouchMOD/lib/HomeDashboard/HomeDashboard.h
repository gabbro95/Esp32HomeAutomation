#ifndef HOME_DASHBOARD_H
#define HOME_DASHBOARD_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <WiFi.h>       
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>

// Includiamo le tue definizioni comuni
#include "MyCommon.h"
#include "MySecrets.h"
#include "MyTimer.h"

// Definizioni Display
// Rotazione display 0
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320
// Rotazione display 1
//#define SCREEN_WIDTH  320
//#define SCREEN_HEIGHT 240
#define MSG_QUEUE_SIZE 10 // Dimensione coda messaggi

class HomeDashboard {
public:
    HomeDashboard();
    
    // Inizializzazione hardware e software
    void begin();
    
    // Loop principale da chiamare nel loop() di Arduino
    void update();

    // Funzione per mostrare un messaggio temporaneo (Pop-up)
    void showToast(const char* text, uint32_t duration_ms = 2000, bool isError = false);

    // Metodo chiamato dall'ISR (Interrupt)
    void handleIncomingMessageISR(const uint8_t *mac, const uint8_t *incomingData, int len);

    // Metodi per inviare comandi 
    void sendMessage(DeviceType destDevice, CommandType command, uint32_t value = 0);

private:
    // --- Oggetti LVGL ---
    lv_obj_t *btn_light_garage, *label_light_garage;
    lv_obj_t *btn_light_small_gate, *label_light_small_gate;
    lv_obj_t *btn_light_extern, *label_light_extern;
    lv_obj_t *btn_gate, *label_gate;
    lv_obj_t *btn_small_gate, *label_small_gate;
    lv_obj_t *btn_call_small_gate, *label_call_small_gate;

    // --- Stato Sistema ---
    GateState gate;
    GarageState garage;
    SmallGateState smallGate;
    LightState lightExtern;
    
    uint32_t sequenceNum = 0;
    bool touchState = false;

    // --- GESTIONE CODA MESSAGGI (FIFO) ---
    // Usiamo una coda circolare per non perdere messaggi ravvicinati (ACK + STATUS)
    EspNowMessage msgQueue[MSG_QUEUE_SIZE];
    volatile int queueHead = 0; // Dove scrive l'ISR
    volatile int queueTail = 0; // Dove legge il Loop

    // --- Timer Interni ---
    MyTimer heartbeatTimer;
    MyTimer linkWatchdog;
    MyTimer spegnimentoDisplay; // Timer per lo spegnimento backlight (ex timerDisplay)
    MyTimer accensioneDisplay;
    MyTimer controlTouch;
    MyTimer linkDisplayTimer;
    MyTimer buzzerTimer;

    // Variabili per il controllo della melodia
    int counter = 0; // Indice della nota corrente da suonare
    bool isPlaying = false; // Stato: la melodia è in riproduzione?
    bool play = false; // Stato: la melodia
    bool isCall = false;    // Stato: chiamata

    // Timer LVGL
    esp_timer_handle_t lvgl_tick_timer;

    // --- Metodi UI Interni ---
    void createGui();
    void updateGateUI();
    void updateLightGarageUI();
    void updateSmallGateUI();
    void updateCallSmallGateUI();
    void updateLigthExternUI();
    void updateLigthSmallGateUI();
    void setNoLinkStatus();

    // Funzione del campanello
    void updateBuzzer();

    // Funzione interna che processa un singolo messaggio (Safe per GUI)
    void processSingleMessage(const EspNowMessage& msg); 

    // --- Callback Eventi UI ---
    static void btn_event_handler_trampoline(lv_event_t * e);

    // Helper per processare i comandi UI
    void processButtonEvent(DeviceType target, CommandType cmd, const char* debugMsg);

    // Setup ESP-NOW
    void setupEspNow();
};

// Riferimento globale
extern HomeDashboard* dashboardInstance;

#endif