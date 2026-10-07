#ifndef ROOM_STATE_H
#define ROOM_STATE_H

#include <vector>
#include "SensorChannel.h"
#include "LightSensor.h"
#include "TempHumiditySensor.h"
#include "MotionSensor.h"
#include "DoorSensor.h"

class RoomState {
public:
    // Channels for various sensors
    SensorChannel<float> temperature;
    SensorChannel<float> humidity;
    SensorChannel<bool> motion;
    SensorChannel<bool> doorOpen;
    
    LightArbiter* lightArbiter;

    bool isDaylight = false;
    unsigned long lastPostMillis = 0;
    unsigned long lastTriggerAllMillis = 0;
    int loopCount = 0;

    RoomState(TempHumiditySensor* th,
               MotionSensor* m,
               DoorSensor* d,
               LightArbiter* arbiter)
        : temperature(th, -99.0f, false, 0.1f),
          humidity(th, -99.0f, false, 0.5f),
          motion(m, false, true),
          doorOpen(d, false, true),
          lightArbiter(arbiter),
          lastPostMillis(0),
          loopCount(0) {}

    void triggerAll() {
        lastTriggerAllMillis = millis();
        temperature.trigger();
        humidity.trigger();
        motion.trigger();
        doorOpen.trigger();
        lightArbiter->requestReading();
    }

    void updateAll() {
        temperature.update([](BaseSensor* s) {
            return ((TempHumiditySensor*)s)->getTemperature();
        });
        humidity.update([](BaseSensor* s) { return ((TempHumiditySensor*)s)->getHumidity(); });
        motion.update([](BaseSensor* s) { return ((MotionSensor*)s)->isMotionDetected(); });
        doorOpen.update([](BaseSensor* s) { return ((DoorSensor*)s)->isDoorOpen(); });
    }

    bool anyChanges() const {
        return temperature.hasChanges()
               || humidity.hasChanges()
               || motion.hasChanges()
               || doorOpen.hasChanges();
    }

    bool allReady() const {
        return temperature.isReady() && humidity.isReady() && 
               motion.isReady() && doorOpen.isReady() && 
               lightArbiter->isReadingReady();
    }

    void clearAllChanges() {
        temperature.clearChanges();
        humidity.clearChanges();
        motion.clearChanges();
        doorOpen.clearChanges();
    }
};

#endif
