#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include "BaseSensor.h"

class MotionSensor : public BaseSensor {
private:
    int pinInput;
    int pinLED;
    bool motionDetected;
    int motionTimer;
    int motionDelay;
    bool ready;

public:
    MotionSensor(int pIn, int pLED, int delaySecs) 
        : pinInput(pIn), pinLED(pLED), motionDetected(false), motionTimer(0), motionDelay(delaySecs), ready(false) {}

    void begin() {
        pinMode(pinInput, INPUT_PULLUP);
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
        if (!digitalRead(pinInput)) {
            motionDetected = true;
            motionTimer = 0;
        } else {
            if (motionTimer < motionDelay) {
                motionTimer++;
            } else {
                motionDetected = false;
            }
        }
        digitalWrite(pinLED, !motionDetected); // low side drive
    }

    bool isMotionDetected() const { return motionDetected; }
};

#endif
