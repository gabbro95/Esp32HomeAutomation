#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "MyTimer.h"
#include "MyCommon.h"   
#include "MySecrets.h"  

#define DEBUG

// Questo dispositivo
const DeviceType THIS_DEVICE_ID = DEV_GATE;

// --- CONFIGURAZIONE PIN ---
const int RELAY_OPEN_PIN = 26;
const int RELAY_CLOSE_PIN = 25;
const int RELAY_FOTO = 27; // Pin usato per l'impulso di stop (fotocellula/sicurezza)
const int LIMIT_SWITCH_OPEN_PIN = 22;
const int LIMIT_SWITCH_CLOSE_PIN = 32;
const int ULTRASONIC_PIN = 23;
//const int ULTRASONIC_ECHO_PIN = 23;
//const int ULTRASONIC_TRIG_PIN = 16;
const int LED_LIMIT_SWITCH_OPEN_PIN = 13;
const int LED_LIMIT_SWITCH_CLOSE_PIN = 14;
const int LED_SENSOR_PIN = 17;
// --- FINE CONFIGURAZIONE PIN ---

// --- DEFINIZIONI SENSORE ---
#define DETECTION_THRESHOLD  150  // cm
#define NUM_SAMPLES          5    // numero di letture per media
#define MAX_VARIATION        5    // tolleranza in cm tra letture consecutive

// Variabili stato
GateState gate;
bool obstacleDetected = false;
bool controlSensor = false; // Flag per attivare la logica del sensore
bool controlSensorState = false; // Stato del sensore per l'autochiusura
uint32_t sequenceNum = 0; 
bool gateOpen = false;
bool gateClose = false;
bool sendStatus = false;

// Timer
MyTimer gateMovementTimeoutTimer; const unsigned long GATE_MOVEMENT_TIMEOUT_MS = 40000;
MyTimer autoCloseTimer; const unsigned long AUTO_CLOSE_DELAY_MS = 10000;
MyTimer limitSwitchTimer; const unsigned long DEBOUNCE_DELAY_MS = 2000;
MyTimer limitSensorTimer; const unsigned long CONTROL_DELAY_MS = 5000;
MyTimer limitControlSensorTimer; const unsigned long CONTROL_ACTIVATE_DELAY_MS = 100;
MyTimer statusReportTimer; const unsigned long HEARTBEAT_INTERVAL_SHORT_MS = HEARTBEAT_INTERVAL_MS / 2;
MyTimer checkInputTimer; const unsigned long INPUT_DELAY_MS = 100;
MyTimer checkSensorTimer;  const unsigned long INPUT_SENSOR_DELAY_MS = 50;


// Forward declaration
void handleGateAction(CommandType command);
void sendStatusUpdate();

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

    if (!gate.isMoving && msg.deviceId == DEV_GATE) handleGateAction(msg.command);

    #ifdef DEBUG
        Serial.printf("[GARAGE] RX: dev=%d cmd=%d val=%d seq=%lu\n",
        msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
    #endif
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    #ifdef DEBUG
        Serial.printf("[GARAGE] TX CB: %s\n", (status == ESP_NOW_SEND_SUCCESS) ? "OK" : "FAIL");
    #endif
}

// Funzione di utilità per simulare l'impulso sui relè
void triggerPin(int PIN, bool state) {
    digitalWrite(PIN, state);
    delay(100);
    digitalWrite(PIN, !state);
    delay(100);
}

// --- LOGICA INTERNA DEL CANCELO ---
// Azioni interne usate solo nel codice del Cancello per mantenere CMD_TOGGLE pulito esternamente
typedef enum : uint8_t {
    ACTION_OPEN,
    ACTION_CLOSE,
    ACTION_STOP
} InternalGateAction;

// Funzione che gestisce le transizioni di stato e l'attivazione dei relè
void handleInternalAction(InternalGateAction action) {
    switch (action) {
        case ACTION_OPEN:
            // Avvia Apertura
            if (gate.gateActual != GATE_ACTUAL_OPENING || gate.gateActual != GATE_ACTUAL_OPEN) {
                // Invia impulso di apertura
                triggerPin(RELAY_OPEN_PIN, LOW); 
                if (gate.gateActual != GATE_ACTUAL_CLOSED)  {
                    digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, LOW);
                    gate.gateActual = GATE_ACTUAL_OPENING; 
                    gateMovementTimeoutTimer.set(GATE_MOVEMENT_TIMEOUT_MS);
                }
                #ifdef DEBUG
                    Serial.println("Azione Interna: Avvio Apertura");
                #endif
            }
            break;
        case ACTION_CLOSE:
            // Avvia Chiusura
            if (gate.gateActual != GATE_ACTUAL_CLOSING) {
                // Invia impulso di chiusura
                triggerPin(RELAY_CLOSE_PIN, LOW); 
                gateClose = true; 
                #ifdef DEBUG
                    Serial.println("Azione Interna: Avvio Chiusura");
                #endif
            }
            break;
        case ACTION_STOP:
            // Ferma il movimento (simula l'impulso foto)
            if (gate.isMoving) {
                // Invia impulso di stop/fotocellula
                triggerPin(RELAY_FOTO, HIGH);
                controlSensorState = false;
                gate.isMoving = false; 
                autoCloseTimer.resetSet();
                gateMovementTimeoutTimer.resetSet();
                gate.gateActual = GATE_ACTUAL_STOPPED;
                #ifdef DEBUG
                    Serial.println("Azione Interna: Cancello Fermato");
                #endif
                sendStatusUpdate();
            }
            break;
    }
}

// Funzione pubblica per i comandi esterni (pulita)
void handleGateAction(CommandType command) {
    switch (command) {
        case CMD_TOGGLE:
            // --- LOGICA PULITA CMD_TOGGLE ---
            if (gate.gateActual == GATE_ACTUAL_CLOSED) {
                handleInternalAction(ACTION_OPEN);
            }
            else if (gate.gateActual == GATE_ACTUAL_OPEN) {
                handleInternalAction(ACTION_CLOSE);
            }
            // Se fermo, riprendi con l'apertura di sicurezza
            else if (gate.gateActual == GATE_ACTUAL_STOPPED) { 
                handleInternalAction(ACTION_CLOSE);
            }
            // Se in movimento, ferma
            else if (gate.gateActual == GATE_ACTUAL_OPENING || gate.gateActual == GATE_ACTUAL_CLOSING) {
                handleInternalAction(ACTION_STOP);
            }
            break;
        default: break;
    }
}

// --- FUNZIONI DI NETWORKING (ESSENZIALI) ---
void sendStatusUpdate() {
    EspNowMessage msg = {};
    msg.deviceId = THIS_DEVICE_ID;
    msg.command = CMD_STATUS;
    msg.gateActual = gate.gateActual;
    msg.stateOn = gate.isMoving;
    msg.sequenceNum = sequenceNum++;
    statusReportTimer.set(HEARTBEAT_INTERVAL_SHORT_MS);

    esp_err_t res = esp_now_send(macCentralMaster, (uint8_t*)&msg, sizeof(msg));
    #ifdef DEBUG
        Serial.printf("📤 Stato inviato (Stato=%u) → %s\n", msg.value, (res == ESP_OK ? "OK" : "ERR"));
    #endif
}

void setupEspNow() {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) { 
        #ifdef DEBUG
            Serial.println("❌ esp_now_init failed"); 
        #endif
        ESP.restart(); 
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
}

// --- LOGICA FINE CORSA ---
void limitSwitch() {
    switch (gate.gateActual) {
        case GATE_ACTUAL_CLOSED:
            // Finecorsa Chiusura Aperto -> Cancello in Apertura
            if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == HIGH) {
                if (!limitSwitchTimer.isSet()) limitSwitchTimer.set(DEBOUNCE_DELAY_MS); 
                if (limitSwitchTimer.check()) {
                    if (!gateOpen) {
                        digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, LOW);
                        gate.gateActual = GATE_ACTUAL_OPENING; 
                        gate.isMoving = true;
                        gateOpen = true;
                        controlSensor = true;
                        gateMovementTimeoutTimer.set(GATE_MOVEMENT_TIMEOUT_MS);
                        sendStatus = true;
                    }
                }
            } else if (limitSwitchTimer.isSet()) limitSwitchTimer.resetSet(); 
            break;
        case GATE_ACTUAL_CLOSING: // In Chiusura
            // Finecorsa Chiusura Chiuso -> Cancello Chiuso (Terminazione)
            if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW) {
                if (!limitSwitchTimer.isSet()) limitSwitchTimer.set(DEBOUNCE_DELAY_MS); 
                if (limitSwitchTimer.check()) {
                    if (gate.isMoving) {
                        digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, HIGH);
                        gate.gateActual = GATE_ACTUAL_CLOSED;
                        controlSensorState = false;
                        gate.isMoving = false; 
                        gateMovementTimeoutTimer.resetSet();
                        limitControlSensorTimer.resetSet();
                        sendStatus = true;
                    }
                }
            } else if (limitSwitchTimer.isSet()) limitSwitchTimer.resetSet(); 
            break;
        case GATE_ACTUAL_OPEN:
            // Finecorsa Apertura Aperto -> Cancello in Chiusura
            if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == HIGH) {
                if (!limitSwitchTimer.isSet()) limitSwitchTimer.set(DEBOUNCE_DELAY_MS); 
                if (limitSwitchTimer.check()) {
                    if (gateClose) {
                        digitalWrite(LED_LIMIT_SWITCH_OPEN_PIN, LOW);
                        gate.gateActual = GATE_ACTUAL_CLOSING; 
                        gate.isMoving = true;
                        gateClose = false;
                        gateMovementTimeoutTimer.set(GATE_MOVEMENT_TIMEOUT_MS);
                        sendStatus = true;
                    }
                }
            } else if (limitSwitchTimer.isSet()) limitSwitchTimer.resetSet(); 
            break;
        case GATE_ACTUAL_OPENING: // In Apertura
            // Finecorsa Apertura Chiuso -> Cancello Aperto (Terminazione)
            if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == LOW) {
                if (!limitSwitchTimer.isSet()) limitSwitchTimer.set(DEBOUNCE_DELAY_MS); 
                if (limitSwitchTimer.check()) {
                    if (gate.isMoving) {
                        digitalWrite(LED_LIMIT_SWITCH_OPEN_PIN, HIGH); 
                        gate.gateActual = GATE_ACTUAL_OPEN; 
                        gate.isMoving = false;
                        gateOpen = false;
                        gateMovementTimeoutTimer.resetSet();
                        
                        // LOGICA SENSORE DI CHIUSURA (originale)
                        if (controlSensor) {
                            if (controlSensorState) {
                                controlSensor = false;
                                autoCloseTimer.set(AUTO_CLOSE_DELAY_MS);
                            }
                        } else autoCloseTimer.set(AUTO_CLOSE_DELAY_MS);
                        sendStatus = true;
                    }
                }
            } else if (limitSwitchTimer.isSet()) limitSwitchTimer.resetSet();
            break;
        default:
            break;
    }
}


void setup() {
    Serial.begin(115200);

    // Setup PIN e stati iniziali
    pinMode(RELAY_OPEN_PIN, OUTPUT); digitalWrite(RELAY_OPEN_PIN, HIGH);
    pinMode(RELAY_CLOSE_PIN, OUTPUT); digitalWrite(RELAY_CLOSE_PIN, HIGH); 
    pinMode(RELAY_FOTO, OUTPUT); digitalWrite(RELAY_FOTO, LOW); 
    pinMode(LED_LIMIT_SWITCH_CLOSE_PIN, OUTPUT);
    pinMode(LED_LIMIT_SWITCH_OPEN_PIN, OUTPUT);
    pinMode(LED_SENSOR_PIN, OUTPUT);
    pinMode(ULTRASONIC_PIN, INPUT_PULLUP);  
    pinMode(LIMIT_SWITCH_CLOSE_PIN, INPUT_PULLUP);
    pinMode(LIMIT_SWITCH_OPEN_PIN, INPUT_PULLUP);

    delay(10);

    // Controlla e imposta lo stato iniziale del finecorsa di chiusura
    if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW) { 
        digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, HIGH); 
        gate.gateActual = GATE_ACTUAL_CLOSED;
    } 

    setupEspNow();
    statusReportTimer.set(HEARTBEAT_INTERVAL_SHORT_MS);
}


void loop() {
    // FINE CORSA
    if (!checkInputTimer.isSet()) checkInputTimer.set(INPUT_DELAY_MS);
    if (checkInputTimer.check()) {    
        limitSwitch();
    }

    // LETTURA SENSORE E LOGICA OSTACOLO
    if (gate.isMoving || gate.gateActual == GATE_ACTUAL_OPEN) {
        if (digitalRead(ULTRASONIC_PIN) == HIGH) {
            if (!checkSensorTimer.isSet()) checkSensorTimer.set(CONTROL_DELAY_MS); 
            if (checkSensorTimer.check()) {
                if (!obstacleDetected) {
                    #ifdef DEBUG
                        Serial.println("Rilevato ostacolo"); 
                    #endif
                    digitalWrite(LED_SENSOR_PIN, HIGH);
                    obstacleDetected = true;
                    limitSensorTimer.set(CONTROL_DELAY_MS);

                    if (controlSensor) {
                        if (!controlSensorState)  controlSensorState = true; 
                    } else {
                        // --- ARRESTO OSTACOLO: ORA USA LA LOGICA INTERNA ---
                        if (gate.gateActual == GATE_ACTUAL_CLOSING) handleInternalAction(ACTION_OPEN);    
                    }

                    if (autoCloseTimer.isSet())  autoCloseTimer.set(AUTO_CLOSE_DELAY_MS);
                }
            }
        } else checkSensorTimer.set(INPUT_SENSOR_DELAY_MS);
    }

    // --- TIMER DI RIMOZIONE OSTACOLO E RIATTIVAZIONE SENSORE ---
    if (limitSensorTimer.isSet() && limitSensorTimer.check()) {
        if (obstacleDetected) {
            obstacleDetected =  false;
            if (gate.gateActual != GATE_ACTUAL_OPENING && gate.gateActual != GATE_ACTUAL_OPEN) { 
                digitalWrite(LED_SENSOR_PIN, LOW); 
            }
            if (controlSensor && controlSensorState) { 
                if (gate.gateActual == GATE_ACTUAL_OPEN) {
                    controlSensor = false; 
                    autoCloseTimer.set(AUTO_CLOSE_DELAY_MS);
                }
            }
        }
    } 
   
    // --- AZIONI TEMPORIZZATE ---

    // Autochiusura
    if (autoCloseTimer.isSet() ) {
        if (digitalRead(LED_SENSOR_PIN))  digitalWrite(LED_SENSOR_PIN, LOW);
        if (autoCloseTimer.check())  handleInternalAction(ACTION_CLOSE); 
    }

    // Timeout Movimento
    if (gate.isMoving && gateMovementTimeoutTimer.check())  handleInternalAction(ACTION_STOP); 
  
    // RAPPORTO DI STATO PERIODICO
    if (statusReportTimer.isSet() && statusReportTimer.check()) {
        sendStatusUpdate();
        statusReportTimer.set(HEARTBEAT_INTERVAL_SHORT_MS);
    }

    if (sendStatus) {
        sendStatus = false;
        sendStatusUpdate();
    }
}
