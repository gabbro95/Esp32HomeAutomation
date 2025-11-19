#pragma once
#include <Arduino.h>

class MyTimer {
public:
    explicit MyTimer(unsigned long intervalMs = 1000)
    : interval(intervalMs), last(millis()) {}

    void setInterval(unsigned long intervalMs) { interval = intervalMs; }
    void reset() { last = millis(); }

    bool checkAndReset() {
        unsigned long now = millis();
        if (now - last >= interval) { last = now; return true; }
        return false;
    }
private:
    unsigned long interval;
    unsigned long last;
};
