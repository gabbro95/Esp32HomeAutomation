#include <Arduino.h>
#include "HomeDashboard.h"

// Istanziamo la classe dashboard
HomeDashboard dashboard;

void setup() {
  Serial.begin(115200);
  
  // Avviamo tutto: Display, Touch, LVGL, ESP-NOW
  dashboard.begin();
}

void loop() {
    // Aggiorniamo la logica (Touch, Timer, Watchdog, LVGL loop)
    dashboard.update();
}