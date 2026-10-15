#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H
 
#include <Arduino.h>
#include "RoomState.h"
 
class AlertManager {
private:
    bool lightAlert = false;
    bool lightAlertUpdate = false;
    bool doorCloudAlert = false;
    bool doorCloudAlertUpdate = false;
    bool beepState = false;
    unsigned long lastBeepToggle = 0;
 
public:
    void evaluate(RoomState& state, bool isDaylight, int lightThresh) {
        // Light Alert Logic (for cloud push)
        bool currentLightAlert = (!isDaylight && !state.motion.getValue()
                                    && (state.lightArbiter->getValue(0) > lightThresh
                                        || state.lightArbiter->getValue(1) > lightThresh));
        
        lightAlert = currentLightAlert;
 
        // Door Cloud Alert Logic (for cloud push)
        bool currentDoorCloudAlert = (!state.motion.getValue() && state.doorOpen.getValue());
        doorCloudAlert = currentDoorCloudAlert;
    }
 
    void updateBeepState(bool doorOpen, int openDuration, int beepDelay) {
        if (doorOpen && openDuration >= beepDelay) {
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
    bool isDoorAlert() const { return doorCloudAlert; }
    bool getBeepState() const { return beepState; }
 
    bool hasNewTrigger() const {
        return (lightAlert && !lightAlertUpdate) || (doorCloudAlert && !doorCloudAlertUpdate);
    }
 
    void updateSyncState() {
        lightAlertUpdate = lightAlert;
        doorCloudAlertUpdate = doorCloudAlert;
    }
};
 
#endif

