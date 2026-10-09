#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

#include <ESP8266HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>

#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#include <Adafruit_Sensor.h>

#include "ScriptConfig.h"
#include "SheetConfig.h"
#include "WifiConfig.h"
#include "SupabaseConfig.h"

#include "BaseSensor.h"
#include "LightSensor.h"
#include "LightArbiter.h"
#include "TempHumiditySensor.h"
#include "MotionSensor.h"
#include "DoorSensor.h"
#include "RoomState.h"
#include "HardwareConfig.h"
#include "ConfigManager.h"
#include "HTTPSClientManager.h"
#include "AlertManager.h"
#include "DaylightManager.h"
#include "ConnectivityManager.h"

// Global Hardware/Config Instances
PinMap pins;
ConfigManager config;
HTTPSClientManager httpsClient;

DaylightManager daylightManager(config.settings.latitude,
                                config.settings.longitude,
                                config.settings.timezone);
ConnectivityManager connectivityManager(config.settings.wifiRetryAbort);

AsyncWebServer server(80);
AsyncEventSource source("/event");

LightSensor light1(pins.lightD1Pin, pins.analogPin, config.settings.lightRead);
LightSensor light2(pins.lightD2Pin, pins.analogPin, config.settings.lightRead);
LightArbiter lightArbiter;
TempHumiditySensor tempHumSensor(pins.htPin);
MotionSensor motionSensor(pins.motionPin, pins.ledMotionPin, config.settings.motionDelay);
DoorSensor doorSensor(pins.doorPin,
                      pins.ledDoorPin,
                      config.settings.doorOpenAlertDelay,
                      config.settings.doorOpenDir);

RoomState roomState(&tempHumSensor, &motionSensor, &doorSensor, &lightArbiter);
AlertManager alertManager;

void syncConfig() {
    lightArbiter.setMeasurementDelay(config.settings.lightRead);
    doorSensor.setAlertDelay(config.settings.doorOpenAlertDelay);
    motionSensor.setMotionDelay(config.settings.motionDelay);
}

void pushUpdate() {
    Serial.println("Pushing update to SSE...");
    StaticJsonDocument<256> doc;
    doc["temperature"] = roomState.temperature.getValue();
    doc["humidity"] = roomState.humidity.getValue();
    doc["door_open"] = roomState.doorOpen.getValue();
    doc["motion"] = roomState.motion.getValue();
    doc["light1"] = roomState.lightArbiter->getValue(0);
    doc["light2"] = roomState.lightArbiter->getValue(1);

    String payload;
    serializeJson(doc, payload);
    source.send(payload.c_str(), "update", millis());
}

void UpdateSupabase() {
    Serial.println("DEBUG: UpdateSupabase() called");
    String url = String(supabase_url) + "/rest/v1/" + String(room_name);
    WiFiClientSecure client_secure;
    client_secure.setInsecure();
    HTTPClient http;
    if (http.begin(client_secure, url)) {
        Serial.println("DEBUG: HTTP begin success");
        http.addHeader("apikey", supabase_anon_key);
        http.addHeader("Authorization", "Bearer " + String(supabase_anon_key));
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Prefer", "return=minimal");
        
        StaticJsonDocument<512> doc;
        doc["room_name"] = room_name;
        doc["Door"] = roomState.doorOpen.getValue();
        doc["Temperature"] = roomState.temperature.getValue();
        doc["Humidity"] = roomState.humidity.getValue();
        doc["Motion"] = roomState.motion.getValue();
        doc["Light1"] = roomState.lightArbiter->getValue(0);
        doc["Light2"] = roomState.lightArbiter->getValue(1);
        doc["LightAlert"] = alertManager.isLightAlert();
        doc["DoorAlert"] = alertManager.isDoorAlert();
        doc["Daylight"] = roomState.isDaylight;
        
        String payload;
        serializeJson(doc, payload);
        Serial.printf("DEBUG: Posting payload: %s\n", payload.c_str());
        int httpCode = http.POST(payload);
        Serial.printf("DEBUG: HTTP POST response code: %d\n", httpCode);
        if (httpCode > 0) {
            String response = http.getString();
            Serial.printf("DEBUG: Response: %s\n", response.c_str());
        } else {
            Serial.printf("DEBUG: HTTP POST failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
        http.end();
    } else {
        Serial.println("DEBUG: HTTP begin failed");
    }
}

void UpdateSheets() {
    String url_string = "/macros/s/" + String(sheet_id)
        + "/exec?room_name=" + String(room_name)
        + "&Door=" + String(roomState.doorOpen.getValue())
        + "&Temperature=" + String(roomState.temperature.getValue())
        + "&Humidity=" + String(roomState.humidity.getValue())
        + "&Motion=" + String(roomState.motion.getValue())
        + "&Light1=" + String(roomState.lightArbiter->getValue(0))
        + "&Light2=" + String(roomState.lightArbiter->getValue(1))
        + "&LightAlert=" + String(alertManager.isLightAlert())
        + "&DoorAlert=" + String(alertManager.isDoorAlert())
        + "&Daylight=" + String(roomState.isDaylight) + "&";
    
    if (httpsClient.connect(host)) {
        String result = httpsClient.get(url_string, host);
        config.loadFromJson(result); 
        syncConfig();
    }
}

String rootProcessor(const String& var) {
    if (var == "ROOM_NAME") return String(room_name);
    return String();
}

void removeSection(String &html, const String &sectionName) {
    String startTag = "<!-- " + sectionName + "_SECTION -->";
    String endTag = "<!-- END_" + sectionName + "_SECTION -->";
    int startIdx = html.indexOf(startTag);
    int endIdx = html.indexOf(endTag);
    if (startIdx != -1 && endIdx != -1) {
        int endTagPos = html.indexOf("-->", endIdx) + 3;
        html.remove(startIdx, endTagPos - startIdx);
    }
}

void handleRoot(AsyncWebServerRequest *request) {
    Serial.println("DEBUG: Web request received: /root");
    roomState.triggerAll();
    if (!LittleFS.exists("/index.html")) {
        request->send(500, "text/plain", "Index file not found");
        return;
    }
    String html = LittleFS.open("/index.html", "r").readString();
    html.replace("{{ROOM_NAME}}", String(room_name));
    
    #ifndef USE_DOOR_SENSOR
        removeSection(html, "DOOR");
    #endif
    #ifndef USE_MOTION_SENSOR
        removeSection(html, "MOTION");
    #endif
    #ifndef USE_LIGHT_SENSORS
        removeSection(html, "LIGHT");
    #endif
    
    request->send(200, "text/html", html);
}

void handleJSON(AsyncWebServerRequest *request) {
    Serial.printf("DEBUG: Web request received: /json IP: %s\n",
                   request->client()->remoteIP().toString().c_str());
    roomState.triggerAll();
    StaticJsonDocument<256> doc;
    doc["temperature"] = roomState.temperature.getValue();
    doc["humidity"] = roomState.humidity.getValue();
    doc["door_open"] = roomState.doorOpen.getValue();
    doc["motion"] = roomState.motion.getValue();
    doc["light1"] = roomState.lightArbiter->getValue(0);
    doc["light2"] = roomState.lightArbiter->getValue(1);

    String payload;
    serializeJson(doc, payload);
    request->send(200, "application/json", payload);
}

String processor(const String& var) {
    if (var == "ROOM_NAME") return String(room_name);
    if (var == "bBeepEnabled") return config.settings.beepEnabled ? "on" : "off";
    if (var == "bDoorOpenDir") return config.settings.doorOpenDir ? "on" : "off";
    if (var == "CntLightOnThresh") return String(config.settings.lightOnThresh);
    if (var == "CntWifiRetryAbort") return String(config.settings.wifiRetryAbort);
    if (var == "tDoorOpenAlertDelay") return String(config.settings.doorOpenAlertDelay);
    if (var == "tDoorOpenBeepDelay") return String(config.settings.doorOpenBeepDelay);
    if (var == "tLightAlertThresh") return String(config.settings.lightAlertThresh);
    if (var == "tLightRead") return String(config.settings.lightRead);
    if (var == "tMotionDelay") return String(config.settings.motionDelay);
    if (var == "tPost") return String(config.settings.postInterval);
    if (var == "homeAlertIP") return config.settings.homeAlertIP;
    if (var == "latitude") return String(config.settings.latitude, 4);
    if (var == "longitude") return String(config.settings.longitude, 4);
    if (var == "timezone") return String(config.settings.timezone);
    return String();
}

void handleConfig(AsyncWebServerRequest *request) {
    if (!LittleFS.exists("/config.html")) {
        request->send(500, "text/plain", "Config file not found");
        return;
    }
    String html = LittleFS.open("/config.html", "r").readString();
    
    html.replace("{{ROOM_NAME}}", String(room_name));
    html.replace("{{BEEP_CHECKED}}", config.settings.beepEnabled ? "checked" : "");
    html.replace("{{T_DOOR_ALERT}}", String(config.settings.doorOpenAlertDelay));
    html.replace("{{T_DOOR_BEEP}}", String(config.settings.doorOpenBeepDelay));
    html.replace("{{CNT_LIGHT_ON}}", String(config.settings.lightOnThresh));
    html.replace("{{T_LIGHT_ALERT}}", String(config.settings.lightAlertThresh));
    html.replace("{{T_LIGHT_READ}}", String(config.settings.lightRead));
    html.replace("{{T_MOTION_DELAY}}", String(config.settings.motionDelay));
    html.replace("{{CNT_WIFI_ABORT}}", String(config.settings.wifiRetryAbort));
    html.replace("{{T_POST}}", String(config.settings.postInterval));
    html.replace("{{HOME_ALERT_IP}}", config.settings.homeAlertIP);

    #ifndef USE_DOOR_SENSOR
        int startDoor = html.indexOf("<!-- DOOR_SECTION -->");
        int endDoor = html.indexOf("<!-- END_DOOR_SECTION -->");
        if (startDoor != -1 && endDoor != -1) html.remove(startDoor, endDoor - startDoor + 25);
    #endif
    #ifndef USE_MOTION_SENSOR
        int startMotion = html.indexOf("<!-- MOTION_SECTION -->");
        int endMotion = html.indexOf("<!-- END_MOTION_SECTION -->");
        if (startMotion != -1 && endMotion != -1) html.remove(startMotion, endMotion - startMotion + 27);
    #endif
    #ifndef USE_LIGHT_SENSORS
        int startLight = html.indexOf("<!-- LIGHT_SECTION -->");
        int endLight = html.indexOf("<!-- END_LIGHT_SECTION -->");
        if (startLight != -1 && endLight != -1) html.remove(startLight, endLight - startLight + 25);
    #endif

    request->send(200, "text/html", html);
}

void handleSave(AsyncWebServerRequest *request) {
    int count = 0;
    bool beepEnabledSeen = false;
    bool doorOpenDirSeen = false;

    for (size_t i = 0; i < request->params(); i++) {
        AsyncWebParameter* p = request->getParam(i);
        if (p) {
            Serial.printf("DEBUG: Saving param %s = %s\n", p->name().c_str(), p->value().c_str());
            if (p->name() == "bBeepEnabled") beepEnabledSeen = true;
            if (p->name() == "bDoorOpenDir") doorOpenDirSeen = true;
            config.updateSetting(p->name(), p->value());
            count++;
        }
    }
    
    if (!beepEnabledSeen) {
        Serial.println("DEBUG: bBeepEnabled missing from request, setting to off");
        config.updateSetting("bBeepEnabled", "off");
    }
    if (!doorOpenDirSeen) {
        Serial.println("DEBUG: bDoorOpenDir missing from request, setting to off");
        config.updateSetting("bDoorOpenDir", "off");
    }
    
    Serial.printf("Saved %d settings\n", count);
    syncConfig();
    request->redirect("/");
}

void recoverI2CBus() {
    Serial.println("DEBUG: Attempting I2C Bus Recovery...");
    
    pinMode(pins.sclPin, OUTPUT);
    pinMode(pins.sdaPin, INPUT_PULLUP);
    
    for (int i = 0; i < 9; i++) {
        digitalWrite(pins.sclPin, LOW);
        delayMicroseconds(5);
        digitalWrite(pins.sclPin, HIGH);
        delayMicroseconds(5);
    }
    
    Serial.println("DEBUG: I2C Bus Recovery complete.");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- RoomMonitor Booting ---");

    ArduinoOTA.begin();
    
    #ifdef USE_AHT_SENSOR
    recoverI2CBus();
    Wire.begin(pins.sdaPin, pins.sclPin);
    #endif

    if (!LittleFS.begin()) Serial.println("LittleFS Mount Failed");
    
    Serial.println("Initializing Light 1...");
    light1.begin();
    Serial.println("Initializing Light 2...");
    light2.begin();
    lightArbiter.addSensor(&light1);
    lightArbiter.addSensor(&light2);
    syncConfig();
    
    Serial.println("Initializing Temp/Hum Sensor...");
    tempHumSensor.begin();
    
    Serial.println("Initializing Motion Sensor...");
    motionSensor.begin();
    
    Serial.println("Initializing Door Sensor...");
    doorSensor.begin();
    
    pinMode(pins.beepPin, OUTPUT);
    
    roomState.triggerAll();
    lightArbiter.requestReading();
    Serial.println("Triggering all sensors...");
    roomState.lastPostMillis = 0; // Force immediate cloud push on boot
    
    Serial.println("Connecting to WiFi...");
    connectivityManager.connect(ssid, password, room_name);
    Serial.println("WiFi connect call returned.");

    if (connectivityManager.isConnected()) {
        configTime(config.settings.timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
        // UpdateSupabase removed from here to let loop() handle it after sensors are ready
    }
    
    server.on("/", handleRoot);
    server.on("/json", handleJSON);
    server.on("/config.html", handleConfig);
    server.on("/save", handleSave);
    server.on("/style.css", [](AsyncWebServerRequest *request){ request->send(LittleFS, "/style.css", "text/css"); });
    server.on("/events", [](AsyncWebServerRequest *request) {
        request->send(new AsyncEventSourceResponse(&source));
    });
    server.begin();
}

void loop() {
    static unsigned long lastUIUpdateMillis = 0;
    
    if (!connectivityManager.isConnected()) {
        connectivityManager.connect(ssid, password, room_name);
    }
    ArduinoOTA.handle();

    roomState.updateAll();
    lightArbiter.update();
    roomState.isDaylight = daylightManager.isDaylight();

    alertManager.evaluate(roomState, roomState.isDaylight, config.settings.lightOnThresh);
    alertManager.updateBeepState(doorSensor.getOpenDuration(), config.settings.doorOpenAlertDelay);
    alertManager.updateHardware(pins.beepPin);
    
    bool bPushNeeded = (alertManager.hasNewTrigger() || roomState.lastPostMillis == 0 || (millis() - roomState.lastPostMillis >= (unsigned long)config.settings.postInterval * 1000UL));
    
    if (bPushNeeded || alertManager.isLightAlert()) {
        if (roomState.lastTriggerAllMillis == 0 || millis() - roomState.lastTriggerAllMillis > 5000) {
            Serial.printf("DEBUG: Refresh triggered. bPushNeeded: %s, LightAlert: %s\n", 
                          bPushNeeded ? "YES" : "NO", 
                          alertManager.isLightAlert() ? "YES" : "NO");
            roomState.triggerAll();
        }
    }
    
    // 1. Cloud Updates: Only on bPushNeeded (Alerts or 1-hour interval)
    if (bPushNeeded && connectivityManager.isConnected()) {
        bool forcePush = (millis() - roomState.lastTriggerAllMillis >= 10000);
        if (roomState.allReady() || forcePush) {
            if (forcePush && !roomState.allReady()) {
                Serial.println(">>> CLOUD FORCE PUSH: Timeout waiting for sensors.");
            } else {
                Serial.printf(">>> CLOUD PUSH: All sensors ready. T=%.1f H=%.1f\n", 
                                  roomState.temperature.getValue(), 
                                  roomState.humidity.getValue());
            }
            UpdateSupabase();
            pushUpdate();
            
            alertManager.updateSyncState();
            roomState.lastPostMillis = millis();
            roomState.clearAllChanges();
        } else {
            Serial.print(">>> CLOUD PENDING: Waiting for sensors. Ready status: ");
            Serial.print(roomState.temperature.isReady() ? "T:" : "T!");
            Serial.print(roomState.humidity.isReady() ? "H:" : "H!");
            Serial.print(roomState.motion.isReady() ? "M:" : "M!");
            Serial.print(roomState.doorOpen.isReady() ? "D:" : "D!");
            Serial.print(roomState.lightArbiter->isReadingReady() ? "L:" : "L!");
            Serial.println();
        }
    }

    // 2. UI-Only Updates: Push to SSE if sensors have changes.
    if (roomState.anyChanges() && (millis() - lastUIUpdateMillis >= 500)) {
        Serial.print("UI PUSH TRIGGERED. Dirty: ");
        if (roomState.temperature.hasChanges()) Serial.print("T ");
        if (roomState.humidity.hasChanges()) Serial.print("H ");
        if (roomState.motion.hasChanges()) Serial.print("M ");
        if (roomState.doorOpen.hasChanges()) Serial.print("D ");
        Serial.println();
        
        Serial.printf("Values: T:%.1f H:%.1f M:%d D:%d L1:%d L2:%d\n", 
            roomState.temperature.getValue(), roomState.humidity.getValue(),
            roomState.motion.getValue(), roomState.doorOpen.getValue(),
            roomState.lightArbiter->getValue(0), roomState.lightArbiter->getValue(1));

        pushUpdate();
        roomState.clearAllChanges();
        lastUIUpdateMillis = millis();
    }
    
    if (connectivityManager.shouldRestart()) ESP.restart();
}
