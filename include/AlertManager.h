#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include <Arduino.h>
#include "RoomState.h"

class AlertManager {
private:
    bool lightAlert = false;
    bool lightAlertUpdate = false;
    bool doorAlert = false;
    bool doorAlertUpdate = false;
    bool beepState = false;
    unsigned long lastBeepToggle = 0;

public:
    void evaluate(RoomState& state, bool isDaylight, int lightThresh) {
        // Light Alert Logic
        bool currentLightAlert = (!isDaylight && !state.motion.getValue()
                                    && (state.lightArbiter->getValue(0) > lightThresh
                                        || state.lightArbiter->getValue(1) > lightThresh));
        
        lightAlert = currentLightAlert;

        // Door Alert Logic
        bool currentDoorAlert = (!state.motion.getValue() && state.doorOpen.getValue());
        doorAlert = currentDoorAlert;
    }

    void updateBeepState(int openDuration, int alertDelay) {
        if (doorAlert && openDuration >= alertDelay) {
            if (millis() - lastBeepToggle >= 500) {
                beepState = !beepState;
                lastBeepToggle = millis();
            }
        } else {
            beepState = false;
        }
    }

    void updateHardware(int beepPin) {
        extern ConfigManager config;
        if (config.settings.beepEnabled) {
            digitalWrite(beepPin, beepState);
        } else {
            digitalWrite(beepPin, LOW);
        }
    }

    bool isLightAlert() const { return lightAlert; }
    bool isDoorAlert() const { return doorAlert; }
    bool getBeepState() const { return beepState; }

    bool hasNewTrigger() const {
        return (lightAlert && !lightAlertUpdate) || (doorAlert && !doorAlertUpdate);
    }

    void updateSyncState() {
        lightAlertUpdate = lightAlert;
        doorAlertUpdate = doorAlert;
    }
};

#endif
