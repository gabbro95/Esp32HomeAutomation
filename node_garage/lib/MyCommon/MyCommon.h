#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <string.h> // Per memcpy

// --- Parametri di timing comuni ---
const unsigned long HEARTBEAT_INTERVAL_MS = 10000;
const unsigned long RETRY_INTERVAL_MS     = 5000;
const uint8_t RETRY_MAX_ATTEMPTS          = 3;
const unsigned long OFFLINE_TIMEOUT_MS    = 15000;
const int MAX_PEERS                       = 10;
const unsigned long OFF_INTERVAL_MS = 60000;
const unsigned long DEBOUNCE_LDR_MS = 60000;
const int RELAY_PIN = 15;     // PIN del relay per accendere la luce
const int LDR_PIN = 33;     // PIN del crepuscolare per il controllo della luce

// --- ID dei dispositivi (DeviceType) ---
typedef enum : uint8_t {
    DEV_CENTRAL = 0,  
	DEV_CENTRAL_MASTER,  
    DEV_GATE,    
	DEV_SMALL_GATE,   
    DEV_GARAGE,     
    DEV_REMOTE,     
    DEV_DISPLAY_CASA,
	DEV_DISPLAY_RUSTICO
} DeviceType;

// --- Tipi di comando (CommandType) ---
typedef enum : uint8_t {
	CMD_TOGGLE = 0,           // Comando di Azione Universale
	CMD_OPEN,                 // Comando di Apertura Cancelletto
	CMD_CALL,                 // Comando di Chiamata Cancelletto
	CMD_STATUS,               // Richiesta di stato
	CMD_PING,                 // Heartbeat / Richiesta link
	CMD_PONG                  // Risposta Heartbeat
} CommandType;

// --- Stato Attuale del Cancello (GateActualState) ---
typedef enum : uint8_t {
	GATE_ACTUAL_CLOSED = 0,     
    GATE_ACTUAL_OPENING,
	GATE_ACTUAL_OPEN,
	GATE_ACTUAL_CLOSING,
	GATE_ACTUAL_STOPPED
} GateActualState;

// --- Stato Attuale del Cancelletto (GateActualState) ---
typedef enum : uint8_t {
	SMALL_GATE_ACTUAL_CLOSED = 0, 
	SMALL_GATE_ACTUAL_OPEN,
} SmallGateActualState;

// --- Struttura del Messaggio ESP-NOW (Max 250 byte) ---
typedef struct __attribute__((packed)) {
	uint8_t version{1};
	DeviceType deviceId{DEV_CENTRAL};
	DeviceType deviceReply{DEV_CENTRAL};
	CommandType command{CMD_STATUS};
	GateActualState gateActual{GATE_ACTUAL_CLOSED};
	SmallGateActualState smallGateActual{SMALL_GATE_ACTUAL_CLOSED}; 
	bool stateOn{0};
	bool stateCall{0};
	bool statePending{0};
	uint32_t value{0};
	uint32_t sequenceNum{0};
} EspNowMessage;

// --- Strutture di Stato Interne (Non inviate via ESP-NOW) ---
// Stato Interno del Garage (utile per la Centrale)
struct GarageState {
	bool isOn = false;
	bool pending = false; // TRUE = comando inviato ma non ancora confermato
};

// Stato Interno del Cancello (utile per la Centrale)
struct GateState {
	GateActualState gateActual = GATE_ACTUAL_CLOSED;
	bool isMoving = false;
	bool pending = false; // comando in corso
};

// Stato Interno del Cancelletto (utile per la Centrale)
struct SmallGateState {
	SmallGateActualState smallGateActual = SMALL_GATE_ACTUAL_CLOSED;
	bool isOn = false;
	bool isCall = false;
	bool pending = false; // comando in corso
};

// Stato Interno del sole
struct LightState {
	bool isOn= false;
	bool isNight= false;
};

// Informazioni sul Peer (Usate solo dalla Centrale)
struct PeerInfo {
	DeviceType id;
	uint8_t mac[6];
	bool online = false;
	uint32_t lastSeenMs = 0;
	uint32_t lastSeq = 0;

	PeerInfo() = default;
	PeerInfo(DeviceType dev, const uint8_t macArr[6])
	: id(dev) {
	    memcpy(mac, macArr, 6);
	}
};

// --- Utility per i MAC Address ---
inline bool macEqual(const uint8_t a[6], const uint8_t b[6]) {
	for (int i=0; i<6; i++) if (a[i] != b[i]) return false;
	return true;
}

// Utility necessaria alla Centrale (main.cpp) per la stampa dei MAC
inline void logMAC(const uint8_t* mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
