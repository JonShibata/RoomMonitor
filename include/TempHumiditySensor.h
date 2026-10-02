#ifndef TEMP_HUMIDITY_SENSOR_H
#define TEMP_HUMIDITY_SENSOR_H

#include <Arduino.h>
#include "BaseSensor.h"
#include <Adafruit_Sensor.h>

#ifdef USE_AHT_SENSOR
  #include <Adafruit_AHTX0.h>
#elif defined(USE_DHT_SENSOR)
  #include <DHT.h>
  #include <DHT_U.h>
#endif

class TempHumiditySensor : public BaseSensor {
private:
    float temperature;
    float humidity;
    bool initialized = false;
    unsigned long lastUpdateMillis = 0;
    int _pin;
    uint32_t retryCount = 0;

#ifdef USE_AHT_SENSOR
    Adafruit_AHTX0 aht;
#elif defined(USE_DHT_SENSOR)
    DHT_Unified* dht = nullptr;
#endif

public:
    TempHumiditySensor(int pin = -1) : temperature(-99.0f), humidity(-99.0f), _pin(pin) {
#ifdef USE_DHT_SENSOR
        dht = new DHT_Unified(pin, DHT22);
#endif
    }

    ~TempHumiditySensor() {
#ifdef USE_DHT_SENSOR
        if (dht) delete dht;
#endif
    }

    void begin() {
#ifdef USE_AHT_SENSOR
        if (aht.begin()) {
            initialized = true;
            Serial.println("AHT sensor initialized.");
        } else {
            initialized = false;
            Serial.println("ERROR: AHT sensor NOT found!");
        }
#elif defined(USE_DHT_SENSOR)
        if (dht) {
            dht->begin();
            initialized = true;
        }
#endif
    }

    void requestReading() override {}

    bool isReadingReady() const override {
        if (!initialized) return true;
        return temperature != -99.0f;
    }

    void update() override {
        if (!initialized) return;

        // Only throttle if we have a valid current reading. 
        // If we are at -99 or just had a failure, we allow retries.
        if (temperature != -99.0f && millis() - lastUpdateMillis < 30000) return;

        sensors_event_t temp_event, hum_event;
#ifdef USE_AHT_SENSOR
        aht.getEvent(&hum_event, &temp_event);
#elif defined(USE_DHT_SENSOR)
        if (dht) {
            dht->temperature().getEvent(&temp_event);
            dht->humidity().getEvent(&hum_event);
        } else return;
#endif

        bool temp_ok = !isnan(temp_event.temperature) && temp_event.temperature > 5.0 && temp_event.temperature < 80.0;
        bool hum_ok = !isnan(hum_event.relative_humidity) && hum_event.relative_humidity > 0.0 && hum_event.relative_humidity < 100.0;

        if (temp_ok && hum_ok) {
            temperature = temp_event.temperature;
            humidity = hum_event.relative_humidity;
            lastUpdateMillis = millis();
            retryCount = 0; // Reset retries on success
        } else {
            retryCount++;
        }
    }

    float getTemperature() const { return temperature; }
    float getHumidity() const { return humidity; }
    uint32_t getRetryCount() const { return retryCount; }
};

#endif
