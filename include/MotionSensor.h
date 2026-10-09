#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include "BaseSensor.h"

class MotionSensor : public BaseSensor {
private:
    int pinInput;
    int pinLED;
    bool motionDetected;
    unsigned long motionStartTime;
    int motionDelay;
    bool ready;

public:
    MotionSensor(int pIn, int pLED, int delaySecs) 
        : pinInput(pIn), pinLED(pLED), motionDetected(false), motionStartTime(0), motionDelay(delaySecs), ready(false) {}

    void begin() {
        pinMode(pinInput, INPUT);
        pinMode(pinLED, OUTPUT);
        digitalWrite(pinLED, HIGH); // set to off (low side drive)
    }

    void requestReading() override {
        ready = true;
    }

    bool isReadingReady() const override {
        return ready;
    }

    void update() {
        if (digitalRead(pinInput)) {
            motionDetected = true;
            motionStartTime = millis();
        } else {
            if (millis() - motionStartTime >= (unsigned long)motionDelay * 1000UL) {
                motionDetected = false;
            }
        }
        digitalWrite(pinLED, !motionDetected); // low side drive
    }

    bool isMotionDetected() const { return motionDetected; }
    void setMotionDelay(int delay) { motionDelay = delay; }
};

#endif
