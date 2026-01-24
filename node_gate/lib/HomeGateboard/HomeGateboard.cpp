#include "HomeGateboard.h"

#define DEBUG

// Puntatore globale alla classe
HomeGateboard* gateboardInstance = nullptr;

// --- Implementazione Classe HomeDashboard ---
HomeGateboard::HomeGateboard() : 
    heartbeatTimer(HEARTBEAT_INTERVAL_MS),
    limitSwitchTimer(LIMIT_DELAY_MS),
    sensorDelayTimer(INPUT_SENSOR_DELAY_MS),
    sensorDelayLedTimer(CONTROL_DELAY_MS),
    autoCloseTimer(AUTO_CLOSE_DELAY_MS)
{
    gateboardInstance = this;
}


void HomeGateboard::begin() {
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

    setupEspNow();

    // Controlla e imposta lo stato iniziale del finecorsa di chiusura
    if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW) { 
        digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, HIGH); 
        gate.gateActual = GATE_ACTUAL_CLOSED;
    } else if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == LOW) { 
        digitalWrite(LED_LIMIT_SWITCH_OPEN_PIN, HIGH); 
        gate.gateActual = GATE_ACTUAL_OPEN;
    } 

}
void HomeGateboard::handleGateAction(EspNowMessage& msg) {
    if (msg.command == CMD_TOGGLE) {
        triggerPin(RELAY_OPEN_PIN, LOW);
        if (autoCloseTimer.check()) autoCloseTimer.stop();
        else if (gate.isMoving) {
            gate.gateActual = GATE_ACTUAL_STOPPED;
            gate.isMoving = false;
        }
        else if (gate.gateActual == GATE_ACTUAL_OPENING || gate.gateActual == GATE_ACTUAL_CLOSING || gate.gateActual == GATE_ACTUAL_STOPPED) {
            gate.isMoving = true;
        }
        else {
            gate.gateActual = GATE_ACTUAL_CLOSED;
            gate.isMoving = false;
        }
    }
    else if (msg.command == CMD_STATUS) {}
}

static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    #ifdef DEBUG
        Serial.printf("[GARAGE] TX CB: %s\n", (status == ESP_NOW_SEND_SUCCESS) ? "OK" : "FAIL");
    #endif
}

static void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
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
    if (gateboardInstance) {
        gateboardInstance->handleGateAction(msg);
    }

    #ifdef DEBUG
        Serial.printf("[GARAGE] RX: dev=%d cmd=%d val=%d seq=%lu\n",
        msg.deviceId, msg.command, msg.value, (unsigned long)msg.sequenceNum);
    #endif
}

void HomeGateboard::setupEspNow() {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ esp_now_init failed");
        ESP.restart();
    }
    esp_now_set_pmk(espNowLtk); 

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macCentralMaster, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = true;
    memcpy(peerInfo.lmk, espNowLtk, 16);

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ esp_now_add_peer failed");
    }
    esp_now_register_recv_cb(onDataRecv);
}

void HomeGateboard::begin() {
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

    setupEspNow();

    // Controlla e imposta lo stato iniziale del finecorsa di chiusura
    if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW) { 
        digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, HIGH); 
        gate.gateActual = GATE_ACTUAL_CLOSED;
    } 

}

// Funzione di utilità per simulare l'impulso sui relè
void HomeGateboard::triggerPin(int PIN, bool state) {
    digitalWrite(PIN, state);
    delay(100);
    digitalWrite(PIN, !state);
    delay(100);
}

// --- FUNZIONI DI NETWORKING (ESSENZIALI) ---
void HomeGateboard::sendStatusUpdate() {
    EspNowMessage msg = {};
    msg.deviceId = THIS_DEVICE_ID;
    msg.command = CMD_STATUS;
    msg.gateActual = gate.gateActual;
    msg.sequenceNum = sequenceNum++;
    heartbeatTimer.reset();

    esp_err_t res = esp_now_send(macCentralMaster, (uint8_t*)&msg, sizeof(msg));
    #ifdef DEBUG
        Serial.printf("📤 Stato inviato (Stato=%u) → %s\n", msg.value, (res == ESP_OK ? "OK" : "ERR"));
    #endif
}
// --- LOGICA FINE CORSA ---
void HomeGateboard::limitSwitch() {
    if (limitSwitchTimer.checkAndReset()) {
        static bool start = false;
        if (gate.gateActual == GATE_ACTUAL_STOPPED) {
            if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW) {
                digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, HIGH);
                gate.gateActual = GATE_ACTUAL_CLOSED;
                gate.isMoving = false;
                controlSensor = false;
                controlSensorState = false;
                start = false;
            } else if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == LOW) {
                digitalWrite(LED_LIMIT_SWITCH_OPEN_PIN, HIGH); 
                gate.gateActual = GATE_ACTUAL_OPEN; 
                gate.isMoving = true;
                autoCloseTimer.reset();
            }
            sendStatusUpdate();
        }
        else if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == HIGH) {
            // Finecorsa Chiusura Aperto -> Cancello in Apertura
            if (GATE_ACTUAL_CLOSED && start) {
                digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, LOW);
                gate.gateActual = GATE_ACTUAL_OPENING; 
                gate.isMoving = true;
                controlSensor = false;
                controlSensorState = false;
                start = true;
                sendStatusUpdate();
            } else start = false; 
        } else if (digitalRead(LIMIT_SWITCH_CLOSE_PIN) == LOW) {
            // Finecorsa Chiusura Chiuso -> Cancello Chiuso (Terminazione)
            if (gate.gateActual == GATE_ACTUAL_CLOSING) {
                digitalWrite(LED_LIMIT_SWITCH_CLOSE_PIN, HIGH);
                gate.gateActual = GATE_ACTUAL_CLOSED;
                gate.isMoving = false;
                controlSensorState = false;
                start = false;
                sendStatusUpdate();
            }
        } else if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == HIGH) {
            // Finecorsa Apertura Aperto -> Cancello in Chiusura
            if (gate.gateActual == GATE_ACTUAL_OPEN) {
                digitalWrite(LED_LIMIT_SWITCH_OPEN_PIN, LOW);
                gate.gateActual = GATE_ACTUAL_CLOSING; 
                if (autoCloseTimer.check()) autoCloseTimer.stop();
                sendStatusUpdate();
            }
        } else if (digitalRead(LIMIT_SWITCH_OPEN_PIN) == LOW) {
            // Finecorsa Apertura Chiuso -> Cancello Aperto (Terminazione)
            if (gate.isMoving && gate.gateActual != GATE_ACTUAL_OPEN) {
                digitalWrite(LED_LIMIT_SWITCH_OPEN_PIN, HIGH); 
                gate.gateActual = GATE_ACTUAL_OPEN; 
                
                // LOGICA SENSORE DI CHIUSURA (originale)
                if (controlSensor) {
                    if (controlSensorState) {
                        controlSensor = false;
                        autoCloseTimer.reset();
                    }
                } else autoCloseTimer.reset();
                sendStatusUpdate();
            }
        }
    }
}

void HomeGateboard::update() {
    limitSwitch();

     // LETTURA SENSORE E LOGICA OSTACOLO
    if (gate.isMoving || gate.gateActual == GATE_ACTUAL_OPEN) {
        if (digitalRead(ULTRASONIC_PIN) == HIGH) {
            if (sensorDelayTimer.checkAndReset()) {
                if (!obstacleDetected) {
                    #ifdef DEBUG
                        Serial.println("Rilevato ostacolo"); 
                    #endif
                    digitalWrite(LED_SENSOR_PIN, HIGH);
                    obstacleDetected = true;
                    sensorDelayLedTimer.reset();

                    if (controlSensor) {
                        if (!controlSensorState) controlSensorState = true; 
                    } else {
                        // --- ARRESTO OSTACOLO: ORA USA LA LOGICA INTERNA ---
                        if (gate.gateActual == GATE_ACTUAL_CLOSING) triggerPin(RELAY_OPEN_PIN, LOW);    
                    }

                    if (autoCloseTimer.check()) autoCloseTimer.reset();
                }
            }
        } else sensorDelayLedTimer.stop(); 
    }

    // --- TIMER DI RIMOZIONE OSTACOLO E RIATTIVAZIONE SENSORE ---
    if (autoCloseTimer.check() && autoCloseTimer.isExpired()) {
        if (obstacleDetected) {
            obstacleDetected =  false;
            if (gate.gateActual != GATE_ACTUAL_OPENING && gate.gateActual != GATE_ACTUAL_OPEN) { 
                digitalWrite(LED_SENSOR_PIN, LOW); 
            }
            if (controlSensor && controlSensorState) { 
                if (gate.gateActual == GATE_ACTUAL_OPEN) {
                    controlSensor = false; 
                    autoCloseTimer.reset();
                }
            }
        }
    } 
   
    // --- AZIONI TEMPORIZZATE ---

    // Autochiusura
    if (autoCloseTimer.check() && autoCloseTimer.isExpired()) {
        if (digitalRead(LED_SENSOR_PIN))  digitalWrite(LED_SENSOR_PIN, LOW);
        if (autoCloseTimer.check()) {
            triggerPin(RELAY_OPEN_PIN, LOW);
            autoCloseTimer.stop();
        }
    }

    
    // RAPPORTO DI STATO PERIODICO
    if (heartbeatTimer.checkAndReset()) {
        sendStatusUpdate();
    }
}