ESP32 Home Automation
Sistema di domotica fai-da-te basato su ESP32, pensato per rendere la casa connessa e automatizzata. Il progetto sfrutta la comunicazione ESP-NOW per far comunicare tra loro i vari moduli in modo rapido, affidabile e senza bisogno di un router Wi-Fi.

L'architettura è di tipo master + node: un controller_master coordina il traffico dei messaggi, gestisce i nodi e ne amministra le automazioni, mentre i vari nodi periferici si occupano di sensori, attuatori e interfacce utente.

✨ Features
📡 Comunicazione ESP-NOW tra master e nodi (bassa latenza, no router)

🧠 Controller master che gestisce traffico messaggi, nodi e automazioni

🚪 Nodi dedicati per cancello, garage e cancello piccolo

🖥️ Display di controllo per stato e comandi

🎛️ Telecomando remoto basato su M5Stack

🧩 Architettura modulare: aggiungere un nodo è semplice

⚙️ Build tramite PlatformIO

🏗️ Architettura
text
                 ┌─────────────────────┐
                 │  controller_master  │
                 │  (ESP32 centrale)   │
                 └──────────┬──────────┘
                            │ ESP-NOW
        ┌───────────────────┼───────────────────┐
        │                   │                   │
   ┌────▼────┐         ┌────▼────┐         ┌────▼─────┐
   │node_gate│         │node_garage│       │node_small_gate│
   └─────────┘         └─────────┘         └──────────┘

   Periferiche di controllo:
   ┌──────────────┐   ┌────────────────────┐   ┌───────────────────┐
   │  DisplayMOD  │   │ M5Stack-esp-now-   │   │ AZ-TouchMOD_      │
   │              │   │ remote             │   │ Rustico           │
   └──────────────┘   └────────────────────┘   └───────────────────┘
controller_master: cuore del sistema, riceve/invia messaggi ESP-NOW, gestisce lo stato dei nodi e le automazioni.

node_gate / node_garage / node_small_gate: nodi periferici che comandano cancelli e garage.

DisplayMOD: modulo display per visualizzare stato e interagire col sistema.

M5Stack-esp-now-remote: telecomando remoto basato su M5Stack.

AZ-TouchMOD_Rustico: modulo basato su AZ-Touch (ESP32 + display touch) per il controllo locale.

🧰 Hardware
Board ESP32 (una per master e una per ogni nodo)

M5Stack per il telecomando remoto

AZ-Touch (ESP32 + display touch)

Display compatibili con il modulo DisplayMOD

Relè / attuatori per pilotare cancelli e garage

Alimentazione adeguata per ogni nodo

⚠️ Nota: i moduli per cancelli e garage comandano dispositivi reali. Presta attenzione a isolamento, alimentazione e sicurezza elettrica.

📂 Struttura del repository
Cartella	Descrizione
controller_master/	Firmware del controller centrale (gestione nodi e automazioni)
node_gate/	Nodo per il cancello principale
node_garage/	Nodo per il garage
node_small_gate/	Nodo per il cancello piccolo
DisplayMOD/	Modulo display di controllo
M5Stack-esp-now-remote/	Telecomando remoto basato su M5Stack
AZ-TouchMOD_Rustico/	Modulo di controllo basato su AZ-Touch
🚀 Installazione e build
Il progetto usa PlatformIO.

Clona il repository:

bash
git clone https://github.com/gabbro95/Esp32HomeAutomation.git
cd Esp32HomeAutomation
Apri la cartella del modulo che ti interessa con VS Code + PlatformIO (es. controller_master).

Configura i parametri (Wi-Fi/ESP-NOW, ID nodo, pin, ecc.) nel file di configurazione del modulo.

Compila e carica sul dispositivo:

bash
pio run -t upload
(Opzionale) Monitora il seriale:

bash
pio device monitor
Ripeti l'operazione per ogni modulo che vuoi installare.

⚙️ Configurazione
Ogni modulo ha i propri parametri (canale ESP-NOW, MAC address del master, ID nodo, pin di I/O, tempi delle automazioni). Controlla i file di config/header all'interno di ciascuna cartella prima di flashare.

📸 Qui screenshot del display, foto dei moduli montati o un video demo del sistema in funzione.

🖼️ Galleria
(Spazio per immagini: display di controllo, M5Stack remote, nodi montati, ecc.)

🛠️ Stato del progetto
Progetto in sviluppo attivo. Le cartelle vengono aggiornate man mano che i moduli vengono migliorati e vengono risolti bug.

🤝 Contributi
Contributi, idee e segnalazioni sono benvenuti! Apri pure una issue o una pull request.

📄 Licenza
Rilasciato sotto licenza MIT. Vedi il file LICENSE per i dettagli.
