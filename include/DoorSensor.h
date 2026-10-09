#ifndef DOOR_SENSOR_H
#define DOOR_SENSOR_H

#include <Arduino.h>
#include "BaseSensor.h"

class DoorSensor : public BaseSensor {
private:
    int pinInput;
    int pinLED;
    bool doorOpen;
    unsigned long doorOpenStartTime;
    int alertDelay;
    bool ready;
    bool openDir;

public:
    DoorSensor(int pIn, int pLED, int aDelay, bool dir) 
        : pinInput(pIn), pinLED(pLED), doorOpen(false), doorOpenStartTime(0), alertDelay(aDelay), ready(false), openDir(dir) {}

    void begin() {
        pinMode(pinInput, INPUT_PULLUP);
        pinMode(pinLED, OUTPUT);
        digitalWrite(pinLED, LOW);
    }

    void requestReading() override {
        ready = true;
    }

    bool isReadingReady() const override {
        return ready;
    }

    void update() {
        if (digitalRead(pinInput) == true) {
            if (!doorOpen) {
                doorOpenStartTime = millis();
            }
            doorOpen = true;
        } else {
            doorOpen = false;
            doorOpenStartTime = 0;
        }
        digitalWrite(pinLED, doorOpen);
    }

    bool isDoorOpen() const { return doorOpen; }
    int getOpenDuration() const { 
        if (!doorOpen) return 0;
        return (int)((millis() - doorOpenStartTime) / 1000);
    }
    void setOpenDir(bool dir) { openDir = dir; }
    void setAlertDelay(int delay) { alertDelay = delay; }
};

#endif
