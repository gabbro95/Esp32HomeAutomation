#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <string.h> // Per memcpy

// Macro calcolo della durata del Timer
#define SEC_TO_MS(s) ((s) * 1000UL)			// RESTITUISCE IL VALORE IN SECONDI
#define MIN_TO_MS(m) ((m) * 60 * 1000UL)	// RESTITUISCE IL VALORE IN MINUTI	

// --- Parametri di timing comuni ---

// Tempi Timer
const unsigned long HEARTBEAT_INTERVAL_MS = SEC_TO_MS(10);
const unsigned long RETRY_INTERVAL_MS     = SEC_TO_MS(5);
const unsigned long OFFLINE_TIMEOUT_MS    = SEC_TO_MS(15);
const unsigned long OFF_LIGHT_INTERVAL_MS = MIN_TO_MS(2);
const unsigned long OFF_TIMER_LIGHT_INTERVAL_MS = MIN_TO_MS(15);
const unsigned long OFF_TIMER_GATE_INTERVAL_MS = MIN_TO_MS(5);
const unsigned long OFF_TIMER_GATE_MOVING_INTERVAL_MS = SEC_TO_MS(30);
const unsigned long DEBOUNCE_LDR_MS = MIN_TO_MS(1);

const int SOGLIA_LUCE_ACCENSIONE = 10; 		// Valore ADC: Se è PIÙ BASSO di questo, accendi la luce (è scuro).
const int SOGLIA_LUCE_SPEGNIMENTO = 100; 	// Valore ADC: Se è PIÙ ALTO di questo, spegni la luce (è giorno).

const uint8_t RETRY_MAX_ATTEMPTS = 3;
const int MAX_PEERS = 10;
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
	CMD_PONG,                 // Risposta Heartbeat
    CMD_ACK                   // Conferma di ricezione comando
} CommandType;

// --- Stato Attuale del Cancello (GateActualState) ---
typedef enum : uint8_t {
	GATE_ACTUAL_CLOSED = 0,     
    GATE_ACTUAL_OPENING,
	GATE_ACTUAL_OPEN,
	GATE_ACTUAL_CLOSING,
	GATE_ACTUAL_STOPPED
} GateActualState;

// --- Stato Attuale del Cancelletto (SmallGateActualState) ---
typedef enum : uint8_t {
	SMALL_GATE_ACTUAL_CLOSED = 0, 
	SMALL_GATE_ACTUAL_OPEN,
} SmallGateActualState;

// --- Struttura del Messaggio ESP-NOW (Max 250 byte) ---
typedef struct __attribute__((packed)) {
	uint8_t version{1};
	DeviceType deviceId{DEV_CENTRAL};
	CommandType command{CMD_STATUS};
	GateActualState gateActual{GATE_ACTUAL_CLOSED};
	SmallGateActualState smallGateActual{SMALL_GATE_ACTUAL_CLOSED}; 
	bool stateOn{0};
	bool stateCall{0};
	bool statePending{0};
	uint32_t value{0};
	uint32_t sequenceNum{0};
} EspNowMessage;

// --- Strutture di Stato Interne ---
// Stato Interno del Garage
struct GarageState {
	bool isOnLight = false;	// luci del Garage
	bool pending = false; 	// TRUE = comando inviato ma non ancora confermato
};

// Stato Interno del Cancello
struct GateState {
	GateActualState gateActual = GATE_ACTUAL_CLOSED;
	bool isMoving = false;	// movimento del Gate
	bool pending = false; 	// comando in corso
};

// Stato Interno del SmallGate
struct SmallGateState {
	SmallGateActualState smallGateActual = SMALL_GATE_ACTUAL_CLOSED;
	bool isOnLight = false;	// TRUE = luci SmallGate accese
	bool isCall = false;	// chiamata da SmallGate
	bool pending = false; 	// comando in corso
};

// Stato Interno del CentralMaster
struct LightState {
	bool isOnLight = false;	// luci CentralMaster
	bool isNight= false;	// controllo lux CentralMaster
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
