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

    bool hasNewTrigger() const {
        return (lightAlert && !lightAlertUpdate) || (doorAlert && !doorAlertUpdate);
    }

    void updateSyncState() {
        lightAlertUpdate = lightAlert;
        doorAlertUpdate = doorAlert;
    }

    void updateBeep(int openCount, int alertDelay, bool beepEnabled, int beepPin) {
        if (doorAlert) {
            if (openCount >= alertDelay) {
                beepState = !beepState;
            } else {
                beepState = false;
            }
        } else {
            beepState = false;
        }
        
        if (beepEnabled) {
            digitalWrite(beepPin, beepState);
        }
    }

    bool isLightAlert() const { return lightAlert; }
    bool isDoorAlert() const { return doorAlert; }
    bool getBeepState() const { return beepState; }
};

#endif
