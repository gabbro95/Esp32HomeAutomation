#pragma once
#include "MyCommon.h"
#include <vector>
#include "MyTimer.h"

class PeerDevice;
class GarageDevice;
class GateDevice;
// Gestione priorità del msg , ritorna 1 se è alta 
enum class MsgPrio : uint8_t { Low = 0, High = 1 };
// Max di msg non prioritari
static const int RETRY_Q_SIZE = 16;
// PendingMessage del PeerManager
struct PendingMessage {
    EspNowMessage msg{};                                    // msgPending
    uint8_t mac[6]{};                                       // Device da contattare
    uint8_t attempts = 0;                                   // tentativi     
    MsgPrio prio = MsgPrio::Low;                            // Priorità del msg
    bool active = false;                                    // il messaggio è in corso?
};
// Gestore PeerManager
class PeerManager {
    public:
        static PeerManager& getInstance();                  // ritorna PeerManager
        // Dichiarazione mac PeerManager
        uint8_t macCentral[6];
        void macCentralMaster();                            // Inizializzazione di macCentral
        
        void addPeer(PeerDevice* peer);                     // aggiunge un nuovo Device a peers

        PeerDevice* findPeerByMac(const uint8_t* mac);      // ricerca del Device tramite mac
        PeerDevice* findPeerByDevice(DeviceType devName);   // ricerca del Device tramite devName

        void clearPendingForDevice(DeviceType devName);     // settaggio false del pending
        uint32_t getNextSequenceNum();                      // ritorna il valore aggironato del numero di messaggi

        void sendPongMessage(const uint8_t* macDevice);           // risposta al Ping
        void sendAck(const uint8_t* macDest);               // invio un messaggio ACK
        void sendFullStateToUI(const uint8_t* uiMac);       // invio lo stato aggiornato del PeerManager e Device
        void sendUpdateState();                             // aggiornamento Display dello stato di PeerManager
        void replyMessage(const EspNowMessage& msg);        // inoltro del messaggio a msg.deviceId
        void sendStatusMessage(const uint8_t* uiMac);       // aggiornamento Display dello stato attraverso devName
        void sendToggleMessage(DeviceType devName);         // invio un messaggio Toggle a devName
        void sendOpenSmallGateMessage();                    // invio un messaggio per aprire il cancello
        void mirrorStatusToUIs(const EspNowMessage& msg);   // invio messaggio ai Display
        
        LightState getState() const { return state; }       // ritorna il PeerManager
        void setState(bool newState);                       // aggiornamento Luci PeerManager
        
        void setTimer();                                    // Settaggio Timer PeerManager 

        void loop();                                        // loop condiviso

    private:
        //  MANAGEMENT
        PeerManager() = default;                            // istanziamento base di PeerManager
        PeerManager(const PeerManager&) = delete;           // metodo eliminazione del PeerManager
        PeerManager& operator=(const PeerManager&) = delete;// eliminazione del PeerManager
        // DEVICE
        std::vector<PeerDevice*> peers;                     // contenitore dei Device

        // GESTIONE

        PendingMessage retryQueue[RETRY_Q_SIZE];            // messaggio in corso
        int retryCount = 0;                                 // numero di messaggi attivi in retryQueue
        uint32_t sequenceNum = 0;                           // numero di messaggi inviati 
        
        MsgPrio classifyPrio(const EspNowMessage& msg);     // gestione della priorità di msg
        
        LightState state;                                   // gestione Luci peerManager

        // OUTPUT Setting
        const int RELAY_PIN = 15;                           // PIN relay Luci PeerManager 
        const int LDR_PIN = 33;                             // PIN LDR, controllo Luci

        MyTimer heartbeatTimer;                             // Timer invio HeartBeat
        MyTimer retryTimer;                                 // Timer invio messaggi in coda
        MyTimer switchOffSecurityTimer;                     // Timer di sicurezza spegnimento Luci
        MyTimer debounceLDRTimer;                           // Timer controllo LDR

        // ESP-NOW FUNCTIONS
        bool sendEspNowMessage(const uint8_t* mac, const EspNowMessage& msg);     // invio un messaggio al macDest
        void sendOrQueue(const uint8_t* mac, const EspNowMessage& msg);      // invia messaggio, se fallisce lo salva in retryQueue
        
        void enqueueRetryPrio(MsgPrio prio, const EspNowMessage& msg);  // gestione della coda in retryQueue
        // Timer Device
        void sendHeartbeat();                               // invio un messaggio d Heartbeat
        void processRetryQueue();                           // processo msgPending 
        void scanOfflinePeers();                            // ricerca Device offline
};
