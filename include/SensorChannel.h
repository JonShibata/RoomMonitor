#ifndef SENSOR_CHANNEL_H
#define SENSOR_CHANNEL_H

#include <Arduino.h>
#include <functional>
#include "BaseSensor.h"

enum class SensorStatus { OK, PENDING, TIMEOUT, ERROR };

template <typename T>
class SensorChannel {
private:
    BaseSensor* sensor;
    T currentValue;
    T lastPushedValue;
    bool dirty;
    bool requested;
    unsigned long requestTime;
    const unsigned long timeoutMs = 5000;
    SensorStatus status;
    bool synchronous;
    T threshold;

public:
    SensorChannel(BaseSensor* s, T initialValue, bool sync = false, T thresh = 0) 
        :
        sensor(s),
        currentValue(initialValue),
        lastPushedValue(initialValue), 
        dirty(false), requested(false),
        requestTime(0),
        status(SensorStatus::OK),
        synchronous(sync),
        threshold(thresh) {}

    void trigger() {
        if (synchronous) return;
        if (!requested) {
            requested = true;
            requestTime = millis();
            status = SensorStatus::PENDING;
            sensor->requestReading();
        }
    }

    void update(std::function<T(BaseSensor*)> getValueFunc) {
        sensor->update();
        
        if (synchronous) {
            T newValue = getValueFunc(sensor);
            if (newValue != currentValue) {
                currentValue = newValue;
                if (hasSignificantChange(newValue, lastPushedValue)) {
                    dirty = true;
                }
            }
            return;
        }

        if (requested) {
            if (millis() - requestTime > timeoutMs) {
                status = SensorStatus::TIMEOUT;
                requested = false;
                return;
            }

            if (sensor->isReadingReady()) {
                T newValue = getValueFunc(sensor);
                if (newValue != currentValue) {
                    currentValue = newValue;
                    if (hasSignificantChange(newValue, lastPushedValue)) {
                        dirty = true;
                    }
                }
                requested = false;
                status = SensorStatus::OK;
            }
        }
    }

    T getValue() const { return currentValue; }
    bool hasChanges() const { return dirty; }
    void clearChanges() { 
        lastPushedValue = currentValue;
        dirty = false; 
    }
    bool isPending() const { return synchronous ? false : requested; }
    SensorStatus getStatus() const { return status; }
    bool isReady() const {
        return synchronous
               || (!requested && (status == SensorStatus::OK
               || status == SensorStatus::TIMEOUT
               || status == SensorStatus::ERROR)); }

private:
    bool hasSignificantChange(T a, T b) {
        if constexpr (std::is_floating_point<T>::value) {
            return std::abs(a - b) > threshold;
        } else {
            return a != b;
        }
    }
};

#endif
