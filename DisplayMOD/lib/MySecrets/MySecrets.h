#pragma once
#include "MyCommon.h"

// --- Parametri di Connessione e Sicurezza ESP-NOW ---
// WIFI_CHANNEL is fine as a constant in the header
const uint8_t WIFI_CHANNEL = 11;

// Use 'extern' for all variables to declare them without defining storage
extern uint8_t espNowLtk[16];

// --- MAC Address dispositivi ---
extern uint8_t macCentralMaster[6];
extern uint8_t macCentral[6];
extern uint8_t macGate[6];
extern uint8_t macSmallGate[6];
extern uint8_t macGarage[6];
extern uint8_t macRemote[6];
extern uint8_t macDisplayCasa[6];
extern uint8_t macDisplayRustico[6];