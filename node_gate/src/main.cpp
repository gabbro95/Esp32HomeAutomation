#include "HomeGateboard.h"

#define DEBUG

// Istanziamo la classe dashboard
HomeGateboard gateboard;

void setup() {
    gateboard.begin();
}


void loop() {
    gateboard.update();
}
