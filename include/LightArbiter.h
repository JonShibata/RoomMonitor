#ifndef LIGHT_ARBITER_H
#define LIGHT_ARBITER_H

#include <vector>
#include "BaseSensor.h"
#include "LightSensor.h"

class LightArbiter : public BaseSensor {
private:
    std::vector<LightSensor*> sensors;
    int currentSensorIdx;
    unsigned long waitStart;
    const unsigned long measurementDelay = 700; 
    bool updating;

public:
    LightArbiter() : currentSensorIdx(-1), waitStart(0), updating(false) {}

    void addSensor(LightSensor* s) {
        sensors.push_back(s);
    }

    void requestReading() override {
        if (!updating) {
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
        for (auto s : sensors) {
            if (!s->isReadingReady()) return false;
        }
        return true;
    }

    void update() override {
        if (!updating) return;

        if (currentSensorIdx >= (int)sensors.size()) {
            updating = false;
            return;
        }

        LightSensor* s = sensors[currentSensorIdx];
        
        // 1. Check for total timeout (sensor missing or hardware hang)
        // Only trigger if we've waited significantly longer than the measurement delay
        if (millis() - waitStart > 2000) {
            // Only log if we are actually forcing a state change
            if (!s->isReadingReady()) {
                Serial.printf("DEBUG: Light sensor %d timeout, forcing ready.\n", currentSensorIdx + 1);
            }
            s->update(); 
            currentSensorIdx++;
            if (currentSensorIdx < (int)sensors.size()) {
                sensors[currentSensorIdx]->requestReading();
                waitStart = millis();
            } else {
                updating = false;
            }
            return;
        }

        // 2. Normal power-up delay (700ms)
        if (millis() - waitStart < measurementDelay) {
            return;
        }

        // 3. Normal read
        s->update(); 
        
        currentSensorIdx++;
        if (currentSensorIdx < (int)sensors.size()) {
            sensors[currentSensorIdx]->requestReading();
            waitStart = millis();
        } else {
            updating = false;
        }
    }
};

#endif
