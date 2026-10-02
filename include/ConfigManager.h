#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "WifiConfig.h"
#include "SheetConfig.h"

class ConfigManager {
public:
    struct Settings {
        bool beepEnabled = true;
        bool doorOpenDir = true;
        int lightOnThresh = 500;
        int wifiRetryAbort = 10;
        int doorOpenAlertDelay = 30;
        int doorOpenBeepDelay = 10;
        int lightAlertThresh = 100;
        int lightRead = 100;
        int motionDelay = 10;
        int postInterval = 3600; 
        String homeAlertIP = "192.168.1.100";
        float latitude = 40.7128;
        float longitude = -74.0060;
        int timezone = -5;
    } settings;

    bool loadFromJson(String json) {
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, json);
        if (error) return false;

        if (doc.containsKey("bBeepEnabled")) settings.beepEnabled = doc["bBeepEnabled"];
        if (doc.containsKey("bDoorOpenDir")) settings.doorOpenDir = doc["bDoorOpenDir"];
        if (doc.containsKey("CntLightOnThresh")) settings.lightOnThresh = doc["CntLightOnThresh"];
        if (doc.containsKey("CntWifiRetryAbort")) settings.wifiRetryAbort = doc["CntWifiRetryAbort"];
        if (doc.containsKey("tDoorOpenAlertDelay")) settings.doorOpenAlertDelay = doc["tDoorOpenAlertDelay"];
        if (doc.containsKey("tDoorOpenBeepDelay")) settings.doorOpenBeepDelay = doc["tDoorOpenBeepDelay"];
        if (doc.containsKey("tLightAlertThresh")) settings.lightAlertThresh = doc["tLightAlertThresh"];
        if (doc.containsKey("tLightRead")) settings.lightRead = doc["tLightRead"];
        if (doc.containsKey("tMotionDelay")) settings.motionDelay = doc["tMotionDelay"];
        if (doc.containsKey("tPost")) settings.postInterval = doc["tPost"];
        if (doc.containsKey("HomeAlertIP")) settings.homeAlertIP = doc["HomeAlertIP"].as<String>();
        if (doc.containsKey("latitude")) settings.latitude = doc["latitude"];
        if (doc.containsKey("longitude")) settings.longitude = doc["longitude"];
        if (doc.containsKey("timezone")) settings.timezone = doc["timezone"];

        return true;
    }

    void updateSetting(const String& key, const String& value) {
        if (key == "bBeepEnabled") {
            settings.beepEnabled = (value == "on");
        } else if (key == "bDoorOpenDir") {
            settings.doorOpenDir = (value == "on");
        } else if (key == "CntLightOnThresh") {
            settings.lightOnThresh = value.toInt();
        } else if (key == "CntWifiRetryAbort") {
            settings.wifiRetryAbort = value.toInt();
        } else if (key == "tDoorOpenAlertDelay") {
            settings.doorOpenAlertDelay = value.toInt();
        } else if (key == "tDoorOpenBeepDelay") {
            settings.doorOpenBeepDelay = value.toInt();
        } else if (key == "tLightAlertThresh") {
            settings.lightAlertThresh = value.toInt();
        } else if (key == "tLightRead") {
            settings.lightRead = value.toInt();
        } else if (key == "tMotionDelay") {
            settings.motionDelay = value.toInt();
        } else if (key == "tPost") {
            settings.postInterval = value.toInt();
        } else if (key == "HomeAlertIP") {
            settings.homeAlertIP = value;
        } else if (key == "latitude") {
            settings.latitude = value.toFloat();
        } else if (key == "longitude") {
            settings.longitude = value.toFloat();
        } else if (key == "timezone") {
            settings.timezone = value.toInt();
        }
    }
};

#endif
