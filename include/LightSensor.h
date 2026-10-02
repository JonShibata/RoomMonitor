#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Arduino.h>
#include "BaseSensor.h"

class LightSensor : public BaseSensor {
private:
    int pinControl;
    int analogPin;
    int currentIntensity;
    bool ready;

public:
    LightSensor(int pControl, int aPin, unsigned long interval) 
        : pinControl(pControl), analogPin(aPin), currentIntensity(0), ready(false) {}

    void begin() {
        pinMode(pinControl, OUTPUT);
        digitalWrite(pinControl, LOW);
    }

    void requestReading() override {
        ready = false;
        digitalWrite(pinControl, HIGH);
    }

    bool isReadingReady() const override {
       return ready;
    }

    void update() override {
        currentIntensity = analogRead(analogPin);
        digitalWrite(pinControl, LOW);
        ready = true;
    }

    int getIntensity() const { return currentIntensity; }
};

#endif
