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
    SensorChannel<int> light1;
    SensorChannel<int> light2;

    bool isDaylight = false;
    unsigned long lastPostMillis = 0;
    unsigned long lastTriggerAllMillis = 0;
    int loopCount = 0;

    RoomState(TempHumiditySensor* th,
              MotionSensor* m,
              DoorSensor* d,
              LightSensor* l1,
              LightSensor* l2)
        : temperature(th, -99.0f, false, 0.1f),
          humidity(th, -99.0f, false, 0.5f),
          motion(m, false, true),
          doorOpen(d, false, true),
          light1(l1, 0),
          light2(l2, 0),
          lastPostMillis(0),
          loopCount(0) {}

    void triggerAll() {
        lastTriggerAllMillis = millis();
        temperature.trigger();
        humidity.trigger();
        motion.trigger();
        doorOpen.trigger();
        // Light sensors are managed by the Arbiter, not triggered individually
    }

    void updateAll() {
        temperature.update([](BaseSensor* s) {
            return ((TempHumiditySensor*)s)->getTemperature();
        });
        humidity.update([](BaseSensor* s) { return ((TempHumiditySensor*)s)->getHumidity(); });
        motion.update([](BaseSensor* s) { return ((MotionSensor*)s)->isMotionDetected(); });
        doorOpen.update([](BaseSensor* s) { return ((DoorSensor*)s)->isDoorOpen(); });
        light1.update([](BaseSensor* s) { return ((LightSensor*)s)->getIntensity(); });
        light2.update([](BaseSensor* s) { return ((LightSensor*)s)->getIntensity(); });
    }


    bool anyChanges() const {
        return temperature.hasChanges()
               || humidity.hasChanges()
               || motion.hasChanges()
               || doorOpen.hasChanges()
               || light1.hasChanges()
               || light2.hasChanges();
    }

    bool allReady() const {
        return temperature.isReady() && humidity.isReady() && 
               motion.isReady() && doorOpen.isReady() && 
               light1.isReady() && light2.isReady();
    }

    void clearAllChanges() {
        temperature.clearChanges();
        humidity.clearChanges();
        motion.clearChanges();
        doorOpen.clearChanges();
        light1.clearChanges();
        light2.clearChanges();
    }
};

#endif
