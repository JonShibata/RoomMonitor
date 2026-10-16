#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H
 
#include <Arduino.h>
#include <ArduinoJson.h>
#include "WifiConfig.h"
 
class ConfigManager {
public:
    struct Settings {
        bool beepEnabled = true;
        bool doorOpenDir = 
        #if defined(basement)
            true;
        #else
            false;
        #endif
        int lightOnThresh = 70;
        int wifiRetryAbort = 10;
        int doorOpenAlertDelay = 300;
        int doorOpenBeepDelay = 20;
        int lightAlertThresh = 600;
        int lightRead = 700;
        int motionDelay = 600;
        int postInterval = 3600; 
        float latitude = 40.7128;
        float longitude = -74.0060;
        int timezone = -5;
        const char* mqtt_broker = "192.168.6.53";
        String mqtt_user = "";
        String mqtt_pass = "";
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
        if (doc.containsKey("latitude")) settings.latitude = doc["latitude"];
        if (doc.containsKey("longitude")) settings.longitude = doc["longitude"];
        if (doc.containsKey("timezone")) settings.timezone = doc["timezone"];
        if (doc.containsKey("mqtt_user")) settings.mqtt_user = doc["mqtt_user"].as<String>();
        if (doc.containsKey("mqtt_pass")) settings.mqtt_pass = doc["mqtt_pass"].as<String>();
 
        return true;
    }


    void updateSetting(const String& key, const String& value) {
        if (key == "bBeepEnabled") {
            settings.beepEnabled = (value == "on");
            Serial.printf("DEBUG: ConfigManager updated bBeepEnabled to %s\n", settings.beepEnabled ? "true" : "false");
        } else if (key == "bDoorOpenDir") {
            settings.doorOpenDir = (value == "on");
            Serial.printf("DEBUG: ConfigManager updated bDoorOpenDir to %s\n", settings.doorOpenDir ? "true" : "false");
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
        } else if (key == "latitude") {
            settings.latitude = value.toFloat();
        } else if (key == "longitude") {
            settings.longitude = value.toFloat();
        } else if (key == "timezone") {
            settings.timezone = value.toInt();
        } else if (key == "mqtt_user") {
            settings.mqtt_user = value;
        } else if (key == "mqtt_pass") {
            settings.mqtt_pass = value;
        }
    }
};

#endif
