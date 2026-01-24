#include "HomeDashboard.h"

#define DEBUG

// Istanza globale del display TFT 
TFT_eSPI tft = TFT_eSPI();

// Puntatore globale alla classe
HomeDashboard* dashboardInstance = nullptr;

// ==========================================================
// FUNZIONE PER SUONARE LA MELODIA DING-DONG
// ==========================================================
void HomeDashboard::updateBuzzer() {
    if (play) {
        if (!isPlaying && counter == 0) {
            isPlaying = true;
            int state = digitalRead(BUZZER_PIN);
            digitalWrite(BUZZER_PIN, !state);
            counter++;
            buzzerTimer.setInterval(INTERVALLO);
        } else if (isPlaying && buzzerTimer.checkAndReset() && counter < RIPETIZIONI) {
            int state = digitalRead(BUZZER_PIN);
            digitalWrite(BUZZER_PIN, !state);
            if (!state)  counter++;
        }
    } else {
        // Se la melodia non è in riproduzione, controlla se è da attivare
        counter = 0;
        isPlaying = false;
        return;
    }
}

// --- Callback statici ---
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_drv_t * indev, lv_indev_data_t *data) {
  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y); 
  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    if (x >= SCREEN_WIDTH)  x = SCREEN_WIDTH - 1;
    if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  }
}

static void lv_tick_task(void* arg) { 
    lv_tick_inc(5); 
}

// Callback ISR di ESP-NOW
static void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    if (dashboardInstance) {
        dashboardInstance->handleIncomingMessageISR(mac, incomingData, len);
    }
}

static void toast_close_cb(lv_timer_t * t) {
    lv_obj_t * obj = (lv_obj_t *)t->user_data;
    if(lv_obj_is_valid(obj)) {
        lv_obj_del(obj);
    }
}

// --- Implementazione Classe HomeDashboard ---
HomeDashboard::HomeDashboard() : 
    heartbeatTimer(HEARTBEAT_INTERVAL_MS),
    linkWatchdog(OFFLINE_TIMEOUT_MS * 2),
    linkWatch(OFFLINE_TIMEOUT_MS),
    spegnimentoRetroDisplay(TIMEOUT_MS_RETRO_DISPLAY_OFF), 
    linkLvDisplayTimer(5),
    buzzerTimer(INTERVALLO),
    timeOutSensorGate(TIMEOUT_MS)
{
    dashboardInstance = this;
}

void HomeDashboard::begin() {
    #ifndef DEBUG
    Serial.println("🚀 Avvio HomeDashboard Library");
    #endif

    pinMode(TFT_BL, OUTPUT); 
    pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(TFT_BL, HIGH);
    pinMode(TFT_IRQ, INPUT_PULLUP);

    lv_init();
    tft.begin();
    tft.setRotation(0);
    // Colori di base per la calibrazione
    uint32_t cal_fg = TFT_WHITE; 
    uint32_t cal_bg = TFT_BLACK;
    tft.setTouch(calData);

    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    createGui(); 

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick"
    };
    esp_timer_create(&periodic_timer_args, &lvgl_tick_timer);
    esp_timer_start_periodic(lvgl_tick_timer, 5000);

    setupEspNow();

    heartbeatTimer.reset();
    linkWatchdog.reset();
    linkLvDisplayTimer.reset();

}

void HomeDashboard::setupEspNow() {
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

void HomeDashboard::showToast(const char* text, uint32_t duration_ms, bool isError) {
    lv_obj_t * toast_obj = lv_obj_create(lv_layer_top());
    lv_obj_set_size(toast_obj, LV_SIZE_CONTENT, 50); 
    lv_obj_set_style_pad_all(toast_obj, 10, 0);
    lv_obj_align(toast_obj, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_color_t bgColor;
    if (isError) {
        bgColor = lv_color_hex(0xAA0000); 
    } else if (strcmp(text, "Centrale: Preso") == 0 || strstr(text, "Centrale") == 0 || strstr(text, nullptr)) {
        bgColor = lv_color_hex(0x0055AA);
    } else {
        bgColor = lv_color_hex(0x00AA00); 
    }

    lv_obj_set_style_bg_color(toast_obj, bgColor, 0);
    lv_obj_set_style_bg_opa(toast_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(toast_obj, 0, 0);
    lv_obj_set_style_radius(toast_obj, 10, 0);

    lv_obj_t * label = lv_label_create(toast_obj);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_center(label);

    lv_timer_t * t = lv_timer_create(toast_close_cb, duration_ms, toast_obj);
    lv_timer_set_repeat_count(t, 1);
}

void HomeDashboard::sendMessage(DeviceType destDevice, CommandType command, uint32_t value) {
    EspNowMessage msg = {};
    msg.deviceId    = destDevice;      
    msg.command     = command;
    msg.value       = value;
    msg.sequenceNum = sequenceNum++;

    esp_err_t res = esp_now_send(macCentralMaster, (uint8_t*)&msg, sizeof(msg));

    if (res == ESP_OK) {
        if (value == 0) showToast("Invio OK\n", 1000, false); 
        else if (value == 9) showToast("Sensore Gate OFF\n", 2000, false); 
        #ifndef DEBUG
        Serial.printf("📤 Send OK dest=%u\n", destDevice);
        #endif
    } else {
        showToast("Errore Invio!\n", 2000, true);
        #ifndef DEBUG
        Serial.println("📤 Send FAIL");
        #endif
    }
}

// --- FUNZIONE ISR (Interrupt Safe) ---
// Inseriamo il messaggio nella Coda (Queue)
void HomeDashboard::handleIncomingMessageISR(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len != sizeof(EspNowMessage)) return;
    
    // Calcolo prossimo indice testa
    int nextHead = (queueHead + 1) % MSG_QUEUE_SIZE;

    // Se la coda non è piena, salviamo il messaggio
    if (nextHead != queueTail) {
        memcpy(&msgQueue[queueHead], incomingData, sizeof(EspNowMessage));
        queueHead = nextHead; // Avanziamo la testa (ora il loop vedrà che head != tail)
    } else {
        // Coda piena: scartiamo (meglio perdere un pacchetto che corrompere la memoria)
        
    }
}

// --- FUNZIONE LOGICA (Safe per GUI) ---
// Processa un singolo messaggio estratto dalla coda
void HomeDashboard::processSingleMessage(const EspNowMessage& msg) {
    
    // GESTIONE ACK DA PEERMANAGER
    if (msg.command == CMD_ACK) {
        showToast("Centrale: Preso", 2000, false); 
        linkWatchdog.reset();
        return;
    }

    if (msg.command == CMD_STATUS) {
        if (msg.statePending == 0) {
            devOff = msg.deviceId;
            linkWatch.reset();
            return;
        }
        if (msg.deviceId == DEV_GARAGE) {
            garage.isOnLight = msg.stateOn;
            updateLightGarageUI();
        } else if (msg.deviceId == DEV_GATE) {
            gate.gateActual = msg.gateActual;
            updateGateUI();
        } else if (msg.deviceId == DEV_SMALL_GATE) {
            smallGate.smallGateActual = msg.smallGateActual;
            smallGate.isOnLight = msg.stateOn;
            updateSmallGateUI();
            updateLigthSmallGateUI();
        } else if (msg.deviceId == DEV_CENTRAL_MASTER) {
            lightExtern.isOnLight = msg.stateOn; 
            updateLigthExternUI();
        }
        linkWatchdog.reset();
    } else if (msg.command == CMD_CALL) {
        updateCallSmallGateUI();
    }
}

void HomeDashboard::processButtonEvent(DeviceType target, CommandType cmd, const char* debugMsg) {
    #ifndef DEBUG
    Serial.println(debugMsg);
    #endif
    if (debugMsg == "Btn Chiamata") {
        isCall = false;
        updateCallSmallGateUI();
        sendMessage(target, cmd, 0);
        return;
    }
    if (!timeOutSensorGate.check()) timeOutSensorGate.reset();
    else if (timeOutSensorGate.check() && timeOutSensorGate.isExpired()) {
        sendMessage(target, cmd, 9);
        timeOutSensorGate.stop();
        return;
    }
    sendMessage(target, cmd, 0);
}

void HomeDashboard::btn_event_handler_trampoline(lv_event_t * e) {
    if (!dashboardInstance) return;
    
    lv_obj_t * btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        if (btn == dashboardInstance->btn_gate) {
            dashboardInstance->processButtonEvent(DEV_GATE, CMD_TOGGLE, "Btn Cancello");
        } else if (btn == dashboardInstance->btn_small_gate) {
            dashboardInstance->processButtonEvent(DEV_SMALL_GATE, CMD_OPEN, "Btn Cancelletto");
        } else if (btn == dashboardInstance->btn_light_garage) {
            dashboardInstance->processButtonEvent(DEV_GARAGE, CMD_TOGGLE, "Btn Luci Garage");
        } else if (btn == dashboardInstance->btn_light_extern) {
            dashboardInstance->processButtonEvent(DEV_CENTRAL_MASTER, CMD_TOGGLE, "Btn Luci Esterne");
        } else if (btn == dashboardInstance->btn_light_small_gate) {
            dashboardInstance->processButtonEvent(DEV_SMALL_GATE, CMD_TOGGLE, "Btn Luci Cancelletto");
        } else if (btn == dashboardInstance->btn_call_small_gate) {
            dashboardInstance->processButtonEvent(DEV_SMALL_GATE, CMD_CALL, "Btn Chiamata");
        }
    }
}

void HomeDashboard::createGui() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 40);
    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview);

    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_20, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_20, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0xAAAAAA), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x153A60), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Cancelli");
    lv_obj_set_style_bg_opa(tab1, LV_OPA_COVER, LV_PART_MAIN); 
    lv_obj_set_style_bg_color(tab1, lv_color_hex(0x3B3B3B), LV_PART_MAIN);
    lv_obj_set_flex_flow(tab1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab1, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(tab1, 20, 0);

    auto createBtn = [&](lv_obj_t* parent, lv_obj_t*& btn, lv_obj_t*& label, const char* text) {
        btn = lv_btn_create(parent);
        lv_obj_set_size(btn, 230, 50);
        lv_obj_add_event_cb(btn, btn_event_handler_trampoline, LV_EVENT_ALL, NULL);
        label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    };

    createBtn(tab1, btn_gate, label_gate, "Cancello: ?");
    updateGateUI();

    createBtn(tab1, btn_small_gate, label_small_gate, "Cancelletto: ?");
    updateSmallGateUI();

    createBtn(tab1, btn_call_small_gate, label_call_small_gate, "Cancelletto: ?");
    updateCallSmallGateUI();

    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Luci");
    lv_obj_set_style_bg_opa(tab2, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tab2, lv_color_hex(0x3B3B3B), LV_PART_MAIN);
    lv_obj_set_flex_flow(tab2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab2, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(tab2, 20, 0);

    createBtn(tab2, btn_light_garage, label_light_garage, "Luci: ?");
    updateLightGarageUI();

    createBtn(tab2, btn_light_extern, label_light_extern, "Luci: ?");
    updateLigthExternUI();

    createBtn(tab2, btn_light_small_gate, label_light_small_gate, "Luci: ?");
    updateLigthSmallGateUI();
}

void HomeDashboard::updateGateUI() {
  lv_color_t color;
  const char *statusText;
  switch (gate.gateActual) {
      case GATE_ACTUAL_CLOSED: color = lv_color_hex(0xAA0000); statusText = "Cancello: CHIUSO"; break;
      case GATE_ACTUAL_OPEN: color = lv_color_hex(0x00AA00); statusText = "Cancello: APERTO"; break;
      case GATE_ACTUAL_OPENING: color = lv_color_hex(0xFF8800); statusText = "Cancello: APERTURA"; break;
      case GATE_ACTUAL_CLOSING: color = lv_color_hex(0xFF8800); statusText = "Cancello: CHIUSURA"; break;
      case GATE_ACTUAL_STOPPED: color = lv_color_hex(0x0088AA); statusText = "Cancello: FERMO"; break;
      default: color = lv_color_hex(0x888888); statusText = "Cancello: SCONOSCIUTO"; break;
  }
  lv_obj_set_style_bg_color(btn_gate, color, 0);
  lv_label_set_text(label_gate, statusText);
}

void HomeDashboard::updateLightGarageUI() {
  if (garage.isOnLight) {
    lv_obj_set_style_bg_color(btn_light_garage, lv_color_hex(0x00AA00), 0);
    lv_label_set_text(label_light_garage, "Garage: ACCESO");
  } else {
    lv_obj_set_style_bg_color(btn_light_garage, lv_color_hex(0xAA0000), 0);
    lv_label_set_text(label_light_garage, "Garage: SPENTO");
  }
}

void HomeDashboard::updateSmallGateUI() {
  if (smallGate.smallGateActual) {
    lv_obj_set_style_bg_color(btn_small_gate, lv_color_hex(0x00AA00), 0);
    lv_label_set_text(label_small_gate, "Cancelletto: APERTO");
  } else {
    lv_obj_set_style_bg_color(btn_small_gate, lv_color_hex(0xAA0000), 0);
    lv_label_set_text(label_small_gate, "Cancelletto.: CHIUSO");
  }
}

void HomeDashboard::updateCallSmallGateUI() {
    if (isCall) {
        lv_obj_set_style_bg_color(btn_call_small_gate, lv_color_hex(0x00AA00), 0);
        lv_label_set_text(label_call_small_gate, "Chiamata in corso");
        play = true;
    } else {
        lv_obj_set_style_bg_color(btn_call_small_gate, lv_color_hex(0xAA0000), 0);
        lv_label_set_text(label_call_small_gate, "Nessuna chiamata");
        play = false;
    }
}

void HomeDashboard::updateLigthExternUI() {
  if (lightExtern.isOnLight) {
    lv_obj_set_style_bg_color(btn_light_extern, lv_color_hex(0x00AA00), 0);
    lv_label_set_text(label_light_extern, "Luci Esterne: ACCESE");
  } else {
    lv_obj_set_style_bg_color(btn_light_extern, lv_color_hex(0xAA0000), 0);
    lv_label_set_text(label_light_extern, "Luci Esterne: SPENTE");
  }
}

void HomeDashboard::updateLigthSmallGateUI() {
  if (smallGate.isOnLight) {
    lv_obj_set_style_bg_color(btn_light_small_gate, lv_color_hex(0x00AA00), 0);
    lv_label_set_text(label_light_small_gate, "Cancelletto: ACCESO");
  } else {
    lv_obj_set_style_bg_color(btn_light_small_gate, lv_color_hex(0xAA0000), 0);
    lv_label_set_text(label_light_small_gate, "Cancelletto: SPENTO");
  }
}

void HomeDashboard::setNoLinkStatusDevice() {
    lv_color_t gray = lv_color_hex(0x888888);
    const char* txt = "NO LINK";
    
    if (devOff == DEV_GARAGE) {
        lv_label_set_text(label_light_garage, txt);
        lv_obj_set_style_bg_color(btn_light_garage, gray, 0);
    }
    
    if (devOff == DEV_GATE) {
        lv_label_set_text(label_gate, txt);
        lv_obj_set_style_bg_color(btn_gate, gray, 0);
    }

    if (devOff == DEV_SMALL_GATE) {
        lv_label_set_text(label_small_gate, txt);
        lv_obj_set_style_bg_color(btn_small_gate, gray, 0);
        lv_label_set_text(label_light_small_gate, txt);
        lv_obj_set_style_bg_color(btn_light_small_gate, gray, 0);
        lv_label_set_text(label_call_small_gate, txt);
        lv_obj_set_style_bg_color(btn_call_small_gate, gray, 0);
    }

}

void HomeDashboard::setNoLinkStatus() {
    lv_color_t gray = lv_color_hex(0x888888);
    const char* txt = "NO LINK";
    
    lv_label_set_text(label_light_garage, txt);
    lv_obj_set_style_bg_color(btn_light_garage, gray, 0);
    
    lv_label_set_text(label_gate, txt);
    lv_obj_set_style_bg_color(btn_gate, gray, 0);
    
    lv_label_set_text(label_small_gate, txt);
    lv_obj_set_style_bg_color(btn_small_gate, gray, 0);
    
    lv_label_set_text(label_light_small_gate, txt);
    lv_obj_set_style_bg_color(btn_light_small_gate, gray, 0);
    
    lv_label_set_text(label_light_extern, txt);
    lv_obj_set_style_bg_color(btn_light_extern, gray, 0);
    
    lv_label_set_text(label_call_small_gate, txt);
    lv_obj_set_style_bg_color(btn_call_small_gate, gray, 0);
}

void HomeDashboard::update() {
    // Gestione Touchscreen e Backlight
    if (spegnimentoRetroDisplay.check() && spegnimentoRetroDisplay.isExpired()) {
        digitalWrite(TFT_BL, HIGH); 
        retroState = false;
        spegnimentoRetroDisplay.stop();
    }

    if (linkLvDisplayTimer.checkAndReset()) {
        lv_timer_handler();
        touchState = !digitalRead(TFT_IRQ);
        if (retroState && touchState) spegnimentoRetroDisplay.reset(); 
        else {
            if (touchState) {
                digitalWrite(TFT_BL, LOW); 
                retroState = true;
                spegnimentoRetroDisplay.reset();
            }
        }
    }

    // --- CONTROLLO CODA MESSAGGI ---
    // Processiamo i messaggi con un LIMITE per non bloccare il loop
    // Se arrivano troppi messaggi insieme, ne leggiamo max 5 per ciclo, 
    // così il resto del codice (touch, spegnimento) viene eseguito subito.
    int processedCount = 0;
    while (queueHead != queueTail && processedCount < 5) {
        // Leggi messaggio dalla coda
        EspNowMessage msg = msgQueue[queueTail];
        
        // Processa la logica e la GUI
        processSingleMessage(msg);
        
        // Avanza la coda (Safe perché l'ISR modifica solo HEAD)
        queueTail = (queueTail + 1) % MSG_QUEUE_SIZE;
        
        processedCount++;
    }

    // AGGIORNA IL BUZZER
    // Questa funzione avanza la melodia se il tempo è scaduto
    updateBuzzer();
    
    // STATUS
    // Heartbeat
    if (heartbeatTimer.isExpired()) {
        sendMessage(DEV_CENTRAL_MASTER, CMD_PING, 0); 
        heartbeatTimer.reset();
    }

    // Watch
    if (linkWatch.check() && linkWatch.isExpired()) {
        setNoLinkStatusDevice();
    }

    // Watchdog
    if (linkWatchdog.isExpired()) {
        setNoLinkStatus();
    }
}