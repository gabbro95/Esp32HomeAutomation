#pragma once
#include <Arduino.h>

class MyTimer {
public:
    MyTimer(unsigned long intervalMs = 1000)
        : interval(intervalMs), last(millis()), state(false) {}

    // imposta un nuovo intervallo
    void setInterval(unsigned long intervalMs) {
        interval = intervalMs;
        state = true;
    }

    // resetta il timer al tempo corrente
    void reset() {
        last = millis();
        state = true;
    }

    // ritorna true se il timer è scaduto
    bool isExpired() {
        unsigned long now = millis();
        if (now - last >= interval) {
            return true;
            state = false;
        }
        return false;
    }

    // ritorna true e resetta se scaduto (modo "one-shot")
    bool checkAndReset() {
        if (isExpired()) {
            reset();
            return true;
        }
        return false;
    }

    // ritorna se il timer è attivo
    bool check() {
        return state;
    }

    // ritorna quanti ms sono passati dall’ultimo reset
    unsigned long elapsed() {
        return millis() - last;
    }

private:
    unsigned long interval;
    unsigned long last;
    bool state;
};
