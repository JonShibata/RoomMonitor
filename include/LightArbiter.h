#ifndef LIGHT_ARBITER_H
#define LIGHT_ARBITER_H

#include <vector>
#include "BaseSensor.h"
#include "LightSensor.h"

class LightArbiter : public BaseSensor {
private:
    std::vector<LightSensor*> sensors;
    std::vector<int> lastValues;
    int currentSensorIdx;
    unsigned long waitStart;
    unsigned long measurementDelay = 700; 
    bool updating;

public:
    LightArbiter() : currentSensorIdx(-1), waitStart(0), updating(false) {}

    void setMeasurementDelay(unsigned long delay) {
        measurementDelay = delay;
    }

    void addSensor(LightSensor* s) {
        sensors.push_back(s);
        lastValues.push_back(0);
    }

    int getValue(int index) const {
        if (index >= 0 && index < (int)lastValues.size()) {
            return lastValues[index];
        }
        return 0;
    }

    void requestReading() override {
        if (!updating) {
            Serial.println("DEBUG: LightArbiter state -> UPDATING (Requested)");
            updating = true;
            currentSensorIdx = 0;
            if (sensors.size() > 0) {
                sensors[currentSensorIdx]->requestReading();
                waitStart = millis();
            }
        }
    }

    bool isReadingReady() const override {
        if (!updating) return true;
        return false;
    }

    void update() override {
        if (!updating) return;

        if (currentSensorIdx >= (int)sensors.size()) {
            Serial.println("DEBUG: LightArbiter state -> IDLE (Finished)");
            updating = false;
            return;
        }

        if (millis() - waitStart < measurementDelay) {
            return;
        }

        LightSensor* s = sensors[currentSensorIdx];
        s->update(); 
        int val = s->getIntensity();
        Serial.printf("DEBUG: LightArbiter read sensor %d: %d\n", currentSensorIdx, val);
        lastValues[currentSensorIdx] = val;
        
        currentSensorIdx++;
        if (currentSensorIdx < (int)sensors.size()) {
            sensors[currentSensorIdx]->requestReading();
            waitStart = millis();
        } else {
            Serial.println("DEBUG: LightArbiter state -> IDLE (All read)");
            updating = false;
        }
    }
};

#endif
