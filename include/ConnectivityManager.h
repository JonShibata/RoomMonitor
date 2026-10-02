#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WifiConfig.h"

class ConnectivityManager {
private:
    int wifiFailCount = 0;
    int retryLimit;

public:
    ConnectivityManager(int limit) : retryLimit(limit) {}

    void connect(const char* ssid, const char* password, const char* hostname) {
        if (WiFi.status() == WL_CONNECTED) return;

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        WiFi.hostname(hostname);
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < retryLimit) {
            delay(1000);
            retries++;
        }

        if (WiFi.status() != WL_CONNECTED) {
            wifiFailCount++;
        } else {
            wifiFailCount = 0;
        }
    }

    bool isConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    int getFailCount() const {
        return wifiFailCount;
    }

    void incrementFail() {
        wifiFailCount++;
    }

    bool shouldRestart() const {
        return wifiFailCount > 10; // This 10 should probably be a variable from config
    }
};

#endif
