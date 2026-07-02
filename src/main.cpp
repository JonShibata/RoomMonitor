#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#include <Adafruit_Sensor.h>

#include "ScriptConfig.h"
#include "SheetConfig.h"
#include "WifiConfig.h"
#include "SupabaseConfig.h"


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

unsigned long tLastUIRead = 0UL;
unsigned long tUIUpdateInterval = 30000; // 30 seconds

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

void UpdateSupabase() {
  String url = String(supabase_url) + "/rest/v1/" + String(room_name);
  
  WiFiClientSecure client_secure;
  client_secure.setInsecure();
  
  HTTPClient http;
  
  Serial.print("Connecting to Supabase: ");
  Serial.println(url);
  
  if (http.begin(client_secure, url)) {
    http.addHeader("apikey", supabase_anon_key);
    http.addHeader("Authorization", "Bearer " + String(supabase_anon_key));
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal");
  
    String payload = "{\"room_name\":\"" + String(room_name) + "\",";
    payload += "\"Door\":" + String(bDoorOpen ? "true" : "false") + ",";
    payload += "\"Temperature\":" + String(T_Ambient) + ",";
    payload += "\"Humidity\":" + String(PctHumidity) + ",";
    payload += "\"Motion\":" + String(bMotion ? "true" : "false") + ",";
    payload += "\"Light1\":" + String(CntLightIntensity1) + ",";
    payload += "\"Light2\":" + String(CntLightIntensity2) + ",";
    payload += "\"LightAlert\":" + String(bLightAlert ? "true" : "false") + ",";
    payload += "\"DoorAlert\":" + String(bDoorAlert ? "true" : "false") + ",";
    payload += "\"LightAlertTrig\":" + String(bLightAlertTrig ? "true" : "false") + ",";
    payload += "\"DoorAlertTrig\":" + String(bDoorAlertTrig ? "true" : "false") + ",";
    payload += "\"Daylight\":" + String(bDaylight ? "true" : "false") + "}";
  
    int httpCode = http.POST(payload);
    
    if (httpCode > 0) {
      Serial.printf("Supabase POST code: %d\n", httpCode);
    } else {
      Serial.printf("Supabase POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void UpdateSupabaseReboot(String message) {
  String url = String(supabase_url) + "/rest/v1/" + String(room_name);
  WiFiClientSecure client_secure;
  client_secure.setInsecure();
  HTTPClient http;
  if (http.begin(client_secure, url)) {
    http.addHeader("apikey", supabase_anon_key);
    http.addHeader("Authorization", "Bearer " + String(supabase_anon_key));
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal");
    String payload = "{\"room_name\":\"" + String(room_name) + "\", \"status_message\":\"" + message + "\"}";
    http.POST(payload);
    http.end();
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
    if (bBeepEnabled) {
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

void UpdateSensorsBlocking() {
  // Force a trigger
  bUpdateTrig = true;
  
  // Start the read process
  #ifdef USE_LIGHT_SENSORS
  ReadLights();
  #endif
  
  #if defined(USE_DHT_SENSOR) || defined(USE_AHT_SENSOR)
  ReadHumidityTemperature();
  #endif
  
  // Clear trigger so we don't restart the process in the loop
  bUpdateTrig = false;
  
  // Wait for completion
  bool bDone = false;
  while (!bDone) {
    bDone = true;
    
    #ifdef USE_LIGHT_SENSORS
    if (!bUpdateLightsCpt) {
      ReadLights();
      bDone = false;
    }
    #endif
    
    #if defined(USE_DHT_SENSOR) || defined(USE_AHT_SENSOR)
    if (!bUpdateTempCpt || !bUpdateHumCpt) {
      ReadHumidityTemperature();
      if (!bUpdateTempCpt || !bUpdateHumCpt) bDone = false;
    }
    #endif
    
    if (!bDone) {
      delay(10);
      yield(); // Allow network stack to process
    }
  }
}

void handleCSS() {
  if (!LittleFS.exists("/style.css")) {
    server.send(404, "text/plain", "CSS file not found");
    return;
  }
  server.send(200, "text/css", LittleFS.open("/style.css", "r").readString());
}

void removeSection(String &html, String startTag, String endTag) {
  int start = html.indexOf(startTag);
  int end = html.indexOf(endTag);
  if (start != -1 && end != -1 && end > start) {
    html.remove(start, end - start + endTag.length());
  }
}

void handleRoot() {
  unsigned long now = millis();
  if (now - tLastUIRead > tUIUpdateInterval) {
    bUpdateTrig = true;
    tLastUIRead = now;
    Serial.println("UI requested sensor update");
  }

  if (!LittleFS.exists("/index.html")) {
    server.send(500, "text/plain", "Index file not found in LittleFS");
    return;
  }
  
  String html = LittleFS.open("/index.html", "r").readString();
  html.replace("{{ROOM_NAME}}", String(room_name));
  html.replace("{{TEMP_F}}", String((T_Ambient * 9.0F / 5.0F) + 32.0F, 1));
  html.replace("{{TEMP_C}}", String(T_Ambient, 1));
  html.replace("{{HUMIDITY}}", String(PctHumidity));
  
  // Basic replacements for status values
  html.replace("{{DOOR_STAT}}", bDoorOpen ? "Open" : "Closed");
  html.replace("{{MOTION_STAT}}", bMotion ? "Detected" : "Clear");
  html.replace("{{LIGHT1}}", String(CntLightIntensity1));
  html.replace("{{LIGHT2}}", String(CntLightIntensity2));
  
  // Conditional section removal
  #ifndef USE_DOOR_SENSOR
    removeSection(html, "<!-- DOOR_SECTION -->", "<!-- END_DOOR_SECTION -->");
  #endif
  
  #ifndef USE_MOTION_SENSOR
    removeSection(html, "<!-- MOTION_SECTION -->", "<!-- END_MOTION_SECTION -->");
  #endif

  #ifndef USE_LIGHT_SENSORS
    removeSection(html, "<!-- LIGHT_SECTION -->", "<!-- END_LIGHT_SECTION -->");
  #endif
  
  server.send(200, "text/html", html);
}

void handleConfig() {
  if (!LittleFS.exists("/config.html")) {
    server.send(500, "text/plain", "Config file not found in LittleFS");
    return;
  }

  String html = LittleFS.open("/config.html", "r").readString();
  
  // Basic replacements
  html.replace("{{ROOM_NAME}}", String(room_name));
  html.replace("{{BEEP_CHECKED}}", bBeepEnabled ? "checked" : "");
  html.replace("{{CNT_WIFI_ABORT}}", String(CntWifiRetryAbort));
  html.replace("{{T_POST}}", String(tPost));
  html.replace("{{HOME_ALERT_IP}}", String(HomeAlertIP));
  
  // Sensor value replacements
  html.replace("{{T_DOOR_ALERT}}", String(tDoorOpenAlertDelay));
  html.replace("{{T_DOOR_BEEP}}", String(tDoorOpenBeepDelay));
  html.replace("{{CNT_LIGHT_ON}}", String(CntLightOnThresh));
  html.replace("{{T_LIGHT_ALERT}}", String(tLightAlertThresh));
  html.replace("{{T_LIGHT_READ}}", String(tLightRead));
  html.replace("{{T_MOTION_DELAY}}", String(tMotionDelay));

  // Conditional section removal
  #ifndef USE_DOOR_SENSOR
    removeSection(html, "<!-- DOOR_SECTION -->", "<!-- END_DOOR_SECTION -->");
  #endif

  #ifndef USE_LIGHT_SENSORS
    removeSection(html, "<!-- LIGHT_SECTION -->", "<!-- END_LIGHT_SECTION -->");
  #endif

  #ifndef USE_MOTION_SENSOR
    removeSection(html, "<!-- MOTION_SECTION -->", "<!-- END_MOTION_SECTION -->");
  #endif
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("bBeepEnabled")) bBeepEnabled = server.arg("bBeepEnabled") == "on";
  if (server.hasArg("CntLightOnThresh")) CntLightOnThresh = server.arg("CntLightOnThresh").toInt();
  if (server.hasArg("CntWifiRetryAbort")) CntWifiRetryAbort = server.arg("CntWifiRetryAbort").toInt();
  if (server.hasArg("tDoorOpenAlertDelay")) tDoorOpenAlertDelay = server.arg("tDoorOpenAlertDelay").toInt();
  if (server.hasArg("tDoorOpenBeepDelay")) tDoorOpenBeepDelay = server.arg("tDoorOpenBeepDelay").toInt();
  if (server.hasArg("tLightAlertThresh")) tLightAlertThresh = server.arg("tLightAlertThresh").toInt();
  if (server.hasArg("tLightRead")) tLightRead = server.arg("tLightRead").toInt();
  if (server.hasArg("tMotionDelay")) tMotionDelay = server.arg("tMotionDelay").toInt();
  if (server.hasArg("tPost")) tPost = server.arg("tPost").toInt();
  if (server.hasArg("HomeAlertIP")) HomeAlertIP = server.arg("HomeAlertIP");

  server.send(200, "text/html", "<html style=\"font-family:sans-serif; text-align:center; padding:50px;\"><h1 style=\"color:#2c3e50\">Settings Saved!</h1><a href=\"/\" style=\"color:#2980b9\">Back to Status</a></body></html>");
}

void handleJSON() {
  UpdateSensorsBlocking();
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

  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
  } else {
    Serial.println("LittleFS Mounted Successfully");
  }

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

  // Perform initial blocking read so first page load isn't zero
  UpdateSensorsBlocking();

  // Send Reboot Notification
  if (WiFi.status() == WL_CONNECTED) {
    String rebootMsg = "Room " + String(room_name) + " has rebooted.";
    UpdateSupabaseReboot(rebootMsg);
  }

  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, 1000, true);

  server.on("/", handleRoot);
  server.on("/json", handleJSON);
  server.on("/config.html", handleConfig);
  server.on("/save", handleSave);
  server.on("/style.css", handleCSS);
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
    if (WiFi.status() == WL_CONNECTED) {
      UpdateSupabaseReboot("Room " + String(room_name) + " internet connection restored.");
    }
  }

  if (bUpdate && bUpdateLightsCpt && bUpdateTempCpt && bUpdateHumCpt &&
      WiFi.status() == WL_CONNECTED) {

    UpdateHomeAlerts();
    // UpdateSheets(); // Removed to stop posting to Google Sheets
    UpdateSupabase();

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
