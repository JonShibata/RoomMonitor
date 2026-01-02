#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#include <Adafruit_Sensor.h>

#include "ScriptConfig.h"
#include "SheetConfig.h"
#include "WifiConfig.h"


// extern "C" {
// #include "user_interface.h"
// }

// HTTPS Redirect -----------------------------------------------------

const int httpsPort = 443;  // HTTPS = 443 and HTTP = 80

HTTPSRedirect* client = nullptr;
ESP8266WebServer server(80);

// Define pin locations
#define iPinDoor 5        // GPIO5:  D1
#define iPinMotion 4      // GPIO4:  D2
#define iPinBeep 0        // GPIO0:  D3
#define iPinLED_Motion 2  // GPIO2:  D4: Built in LED
#define iPinHT 14         // GPIO14: D5
#define iPinLightD1 12    // GPIO12: D6
#define iPinLightD2 13    // GPIO13: D7
#define iPinLED_Door 15   // GPIO15: D8

// Wiring Info-----------------
// Door     GND,            D1
// PIR      GND, Vin (+5V), D2
// Buzzer   GND,            D3
// HT       GND, 3.3V,      D5
// Light1   GND, A0         D6
// Light2   GND, A0         D7

#ifdef USE_AHT_SENSOR
  #include <Adafruit_AHTX0.h>
  Adafruit_AHTX0 aht;
  #define iPinSDA 12 // GPIO12: D6
  #define iPinSCL 13 // GPIO13: D7
#elif defined(USE_DHT_SENSOR)
  #include <DHT.h>
  #include <DHT_U.h>
  DHT_Unified dht(iPinHT, DHT22);
#endif

bool bBeep = false;

bool bMotion = false;
bool bMotionUpdate = false;
bool bMotionTrigger = false;

bool bDoorOpen = false;
bool bDoorAlertUpdate = false;
bool bDoorAlert = false;
bool bDoorAlertTrig = false;

bool bDaylight = false;
bool bLightAlert = false;
bool bLightAlertTrig = false;
bool bLightAlertUpdate = false;

bool bUpdate = false;
bool bUpdateTrig = false;
bool bUpdateTempCpt = false;
bool bUpdateHumCpt = false;
bool bUpdateLightsCpt = false;

int CntDoorOpen = 0;

int CntLoops = 0;
int CntWifiFail = 0;

int CntLightIntensity1 = 0;
int CntLightIntensity2 = 0;

int CntMotionTimer = 0;

float PctHumidity = 0.0F;
float T_Ambient = 0.0F;

unsigned long tReadLightsStart = 0UL;
unsigned long tWiFiConnectStart = 0UL;

os_timer_t myTimer;

sensors_event_t humidity, temperature;

//
//
// Function to read temperature and humidity from the DHT


void ReadHumidityTemperature() {

  if (bUpdateTrig) {
    bUpdateTempCpt = false;
    bUpdateHumCpt = false;
    T_Ambient = -99.0F;
    PctHumidity = -99.0F;
    #ifdef USE_AHT_SENSOR
      aht.getEvent(&humidity, &temperature);
    #elif defined(USE_DHT_SENSOR)
      dht.humidity().getEvent(&humidity);
      dht.temperature().getEvent(&temperature);
    #endif
  }

  if (!isnan(temperature.temperature) && temperature.temperature > -90.0) {
    T_Ambient = temperature.temperature;
    bUpdateTempCpt = true;
  }

  if (!isnan(humidity.relative_humidity) && humidity.relative_humidity > -90.0) {
    PctHumidity = humidity.relative_humidity;
    bUpdateHumCpt = true;
  }
}


void ReadLights() {
  // Multiplexed to analog input
  // Digital outputs used to control which sensor is reporting

  unsigned long dtReadLights;
  unsigned long run_time = millis();

  if (bUpdateTrig || (bLightAlert && bUpdateLightsCpt)) {
    tReadLightsStart = run_time;
    bUpdateLightsCpt = false;
  }

  dtReadLights = run_time - tReadLightsStart;

  if (dtReadLights < (unsigned long)tLightRead) {
    digitalWrite(iPinLightD1, HIGH);
    digitalWrite(iPinLightD2, LOW);
    CntLightIntensity1 = analogRead(A0);
  } else if (dtReadLights < (unsigned long)(tLightRead * 2)) {
    digitalWrite(iPinLightD1, LOW);
    digitalWrite(iPinLightD2, HIGH);
    CntLightIntensity2 = analogRead(A0);
  } else {
    bUpdateLightsCpt = true;
    digitalWrite(iPinLightD1, LOW);
    digitalWrite(iPinLightD2, LOW);
  }
}


void ConnectToWiFi() {

  int CntWifiRetries = 0;
  int intWiFiCode;

  WiFi.mode(WIFI_STA);
  intWiFiCode = WiFi.begin(ssid, password);
  WiFi.hostname(room_name);

  Serial.println("");
  Serial.println("Connecting to WiFi");
  Serial.println("");
  Serial.printf("WiFi.begin = %d\n", intWiFiCode);

  while ((WiFi.status() != WL_CONNECTED) && (CntWifiRetries < CntWifiRetryAbort)) {
    CntWifiRetries++;
    delay(1000);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi NOT Connected");
  } else {
    Serial.println("");
    Serial.println("WiFi Connected");


    // Port defaults to 8266
    // ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(room_name);

    // No authentication by default
    // ArduinoOTA.setPassword("admin");

    // Password can be set with it's md5 value as well
    // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
    // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_FS
        type = "filesystem";
      }

      // NOTE: if updating FS this would be the place to unmount FS using FS.end()
      Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });
    ArduinoOTA.begin();
  }
}


int GetHTTP_String(String* strURL, String* strReturn) {

  HTTPClient http;
  WiFiClient client;

  http.begin(client, *strURL);
  int httpCode = http.GET();
  Serial.println("httpCode=" + String(httpCode));

  if (httpCode > 0) {
    *strReturn = http.getString();
    Serial.println(*strReturn);
  }

  http.end();
  return httpCode;
}


void GetHTTPS_String(String* strURL, String* strReturn) {

  // Use HTTPSRedirect class to create a new TLS connection
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  // client->setPrintResponseBody(true);
  // client->setContentTypeHeader("application/json");

  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  bool flag = false;

  for (int i = 0; i < 5; i++) {
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
      flag = true;
      break;
    } else
      Serial.println("Connection failed. Retrying...");
  }

  if (flag) {

    client->GET(*strURL, host);

    *strReturn = client->getResponseBody();
    Serial.println(*strReturn);

  } else {

    Serial.print("Could not connect to server: ");
    Serial.println(host);
    Serial.println("Exiting...");
  }

  delete client;
  client = nullptr;
}


void FindBoolInString(String* strMain, String strFind, bool* return_val) {

  int iStart = strMain->indexOf(strFind);

  if (iStart != -1) {
    int iEnd = strMain->indexOf(",", iStart);
    int lenFind = (int)strFind.length();

    String return_str = strMain->substring(iStart + lenFind, iEnd);

    Serial.println(strFind + return_str);

    *return_val = (bool)return_str.toInt();
  }
}

void FindIntInString(String* strMain, String strFind, int* return_val) {

  int iStart = strMain->indexOf(strFind);

  if (iStart != -1) {
    int iEnd = strMain->indexOf(",", iStart);
    int lenFind = (int)strFind.length();

    String return_str = strMain->substring(iStart + lenFind, iEnd);

    Serial.println(strFind + return_str);

    *return_val = return_str.toInt();
  }
}

void UpdateSheets() {

  String url_string;
  String strReturn;

  url_string = "/macros/s/" + String(sheet_id) + "/exec?room_name=" + String(room_name) +
      "&Door=" + String(bDoorOpen) + "&Temperature=" + String(T_Ambient) +
      "&Humidity=" + String(PctHumidity) + "&Motion=" + String(bMotion) +
      "&Light1=" + String(CntLightIntensity1) + "&Light2=" + String(CntLightIntensity2) +
      "&LightAlert=" + String(bLightAlert) + "&DoorAlert=" + String(bDoorAlert) +
      "&LightAlertTrig=" + String(bLightAlertTrig) + "&DoorAlertTrig=" + String(bDoorAlertTrig) +
      "&Daylight=" + String(bDaylight) + "&";

  Serial.println(url_string);
  Serial.println();

  GetHTTPS_String(&url_string, &strReturn);

  FindBoolInString(&strReturn, "bBeepEnabled\":", &bBeepEnabled);
  FindBoolInString(&strReturn, "bDaylight\":", &bDaylight);
  FindBoolInString(&strReturn, "bDoorOpenDir\":", &bDoorOpenDir);

  FindIntInString(&strReturn, "CntLightOnThresh\":", &CntLightOnThresh);
  FindIntInString(&strReturn, "CntWifiRetryAbort\":", &CntWifiRetryAbort);

  FindIntInString(&strReturn, "tDoorOpenAlertDelay\":", &tDoorOpenAlertDelay);
  FindIntInString(&strReturn, "tDoorOpenBeepDelay\":", &tDoorOpenBeepDelay);
  FindIntInString(&strReturn, "tLightAlertThresh\":", &tLightAlertThresh);
  FindIntInString(&strReturn, "tLightRead\":", &tLightRead);
  FindIntInString(&strReturn, "tMotionDelay\":", &tMotionDelay);
  FindIntInString(&strReturn, "tPost\":", &tPost);
}


void UpdateHomeAlerts() {

  String s = "http://" + HomeAlertIP;
  s += "/bDoorAlert" + String(room_name) + "=" + String(bDoorAlert) + "&";
  s += "bLightAlert" + String(room_name) + "=" + String(bLightAlert) + "&";

  String strReturn;
  GetHTTP_String(&s, &strReturn);

  Serial.println(s);
  Serial.println("strReturn:");
  Serial.println(strReturn);
}


void timerCallback(void* pArg) {  // timer1 interrupt 1Hz

  // CntLoopsPost = number of seconds before making a new post
  if (CntLoops < tPost) {
    CntLoops++;
  }

  Serial.printf(" CntLoops = %d", CntLoops);
  Serial.printf(" tPost = %d", tPost);
  Serial.printf(" bUpdate = %d", bUpdate);

  #ifdef USE_LIGHT_SENSORS
    Serial.printf(" bUpdateLightsCpt = %d", bUpdateLightsCpt);
  #endif

  #if defined(USE_AHT_SENSOR) || defined(USE_DHT_SENSOR)
    Serial.printf(" bUpdateTempCpt = %d", bUpdateTempCpt);
    Serial.printf(" bUpdateHumCpt = %d\n", bUpdateHumCpt);
  #endif

  #ifdef USE_LIGHT_SENSORS
    Serial.printf(" bLightAlert = %d", bLightAlert);
    Serial.printf(" bLightAlertUpdate = %d", bLightAlertUpdate);
  #endif

  #ifdef USE_DOOR_SENSOR
    bool bDoorLED;
    // bDoorOpenDir = DIO state when door is open (depends on sensor type)
    if (digitalRead(iPinDoor) == bDoorOpenDir) {
      bDoorLED = true;
      bDoorOpen = true;

      // tDoorOpenAlertDelay = number of seconds when door is open before beeping starts
      if (CntDoorOpen < tDoorOpenAlertDelay) {
        // Count up until beep delay expires
        CntDoorOpen++;
      } else {
        // Door has been open longer than delay cal
        // cycle the audible alert
        bBeep = !bBeep;
      }
    } else {
      bBeep = false;
      bDoorLED = false;
      bDoorOpen = false;
      CntDoorOpen = 0;
    }
    if (bBeepEnabled) {}
      digitalWrite(iPinBeep, bBeep);
    }
    digitalWrite(iPinLED_Door, bDoorLED);
    Serial.printf(" bDoorOpen = %d", bDoorOpen);
    Serial.printf(" bDoorAlertUpdate = %d", bDoorAlertUpdate);
  #endif

  #ifdef USE_MOTION_SENSOR
  bool bMotionLED;
  if (digitalRead(iPinMotion)) {
    bMotion = true;
    bMotionLED = true;
    CntMotionTimer = 0;
  } else {
    bMotionLED = false;
    // tMotionDelay = seconds to latch motion detection
    if (CntMotionTimer < tMotionDelay) {
      CntMotionTimer++;
    } else {
      bMotion = false;
    }
  }
  digitalWrite(iPinLED_Motion, !bMotionLED);  // set LED (low side drive)
  Serial.printf(" bMotion = %d", bMotion);
  Serial.printf(" bMotionUpdate = %d", bMotionUpdate);
  #endif
  Serial.printf("\n\n");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv=\"refresh\" content=\"30\">";
  html += "<style>body { font-family: sans-serif; font-size: 1.5rem; padding: 20px; } h1 { font-size: 2rem; }</style>";
  html += "<title>" + String(room_name) + "</title></head><body>";
  html += "<h1>" + String(room_name) + " Status</h1>";
  html += "<p>Temperature: " + String(T_Ambient) + " &deg;C</p>";
  html += "<p>Humidity: " + String(PctHumidity) + " %</p>";
  
  #ifdef USE_DOOR_SENSOR
  html += "<p>Door Open: " + String(bDoorOpen ? "Yes" : "No") + "</p>";
  #endif
  
  #ifdef USE_MOTION_SENSOR
  html += "<p>Motion: " + String(bMotion ? "Yes" : "No") + "</p>";
  #endif

  #ifdef USE_LIGHT_SENSORS
  html += "<p>Light 1: " + String(CntLightIntensity1) + "</p>";
  html += "<p>Light 2: " + String(CntLightIntensity2) + "</p>";
  #endif
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleJSON() {
  String json = "{";
  json += "\"temperature\": " + String(T_Ambient) + ",";
  json += "\"humidity\": " + String(PctHumidity);
  
  #ifdef USE_DOOR_SENSOR
  json += ",\"door_open\": " + String(bDoorOpen ? "true" : "false");
  #endif
  
  #ifdef USE_MOTION_SENSOR
  json += ",\"motion\": " + String(bMotion ? "true" : "false");
  #endif

  #ifdef USE_LIGHT_SENSORS
  json += ",\"light1\": " + String(CntLightIntensity1);
  json += ",\"light2\": " + String(CntLightIntensity2);
  #endif
  
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  #ifdef USE_AHT_SENSOR
    aht.begin();
    Wire.begin(iPinSDA, iPinSCL);
  #elif defined(USE_DHT_SENSOR)
    dht.begin();
  #endif

  #ifdef USE_LIGHT_SENSORS 
    pinMode(iPinLightD1, OUTPUT);
    pinMode(iPinLightD2, OUTPUT);
  #endif

  #ifdef USE_DOOR_SENSOR
    pinMode(iPinDoor, INPUT_PULLUP);
    pinMode(iPinLED_Door, OUTPUT);
    digitalWrite(iPinLED_Door, false);
  #endif

  #ifdef USE_MOTION_SENSOR
    pinMode(iPinMotion, INPUT);
    pinMode(iPinLED_Motion, OUTPUT);
    digitalWrite(iPinLED_Motion, true);  // set to off (low side drive)
  #endif

  pinMode(iPinBeep, OUTPUT);

  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, 1000, true);

  server.on("/", handleRoot);
  server.on("/json", handleJSON);
  server.onNotFound([]() { server.send(404, "text/plain", "404: Not Found"); });
  server.begin();
  Serial.println("HTTP server started");
}


void loop() {
  server.handleClient();

  bool bUpdatePrev = bUpdate;

  #ifdef USE_LIGHT_SENSORS
    bLightAlert =
        (!bDaylight && !bMotion &&
         (CntLightIntensity1 > CntLightOnThresh || CntLightIntensity2 > CntLightOnThresh));

    bLightAlertTrig = bLightAlert && !bLightAlertUpdate;
  #endif
  
  #if defined(USE_DOOR_SENSOR) && defined(USE_MOTION_SENSOR)
    bDoorAlert = !bMotion && bDoorOpen;
    bDoorAlertTrig = bDoorAlert && !bDoorAlertUpdate;
    bMotionTrigger = bMotion && !bMotionUpdate;
  #endif

  bUpdate =
      (bLightAlertTrig || bDoorAlertTrig || bMotionTrigger ||
       (CntLoops >= tPost));

  bUpdateTrig = bUpdate && !bUpdatePrev;
  
  #ifdef USE_LIGHT_SENSORS
    if (bUpdate || bLightAlert) {
      ReadLights();
    }
  #else
    bUpdateLightsCpt = true;
  #endif

  #if defined(USE_DHT_SENSOR) || defined(USE_AHT_SENSOR)
  if (bUpdate) {
    ReadHumidityTemperature();
  }
  #else
    bUpdateTempCpt = true;
    bUpdateHumCpt = true;
  #endif

  if (WiFi.status() != WL_CONNECTED) {
    ConnectToWiFi();
  }

  if (bUpdate && bUpdateLightsCpt && bUpdateTempCpt && bUpdateHumCpt &&
      WiFi.status() == WL_CONNECTED) {

    UpdateHomeAlerts();
    UpdateSheets();

    bDoorAlertUpdate = bDoorAlert;
    bMotionUpdate = bMotion;
    bLightAlertUpdate = bLightAlert;
    CntLoops = 0;
  }

  if (CntWifiFail > CntWifiFailThresh) {
    ESP.restart();
  }
  ArduinoOTA.handle();
}
