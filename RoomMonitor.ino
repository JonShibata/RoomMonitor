
#include "ScriptConfig.h"
#include "SheetConfig.h"
#include "WifiConfig.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>


extern "C" {
#include "user_interface.h"
}

WiFiClient wificlient;

// HTTPS Redirect -----------------------------------------------------

const int httpsPort = 443;  // HTTPS = 443 and HTTP = 80

HTTPSRedirect* client = nullptr;


// TODO: Update IFTTT for door state change and lights on / off at night
// TODO: replace delays with timers, check results after timer expire


// Define pin locations

#define iPinDoor 5        // GPIO5:  D1
#define iPinMotion 4      // GPIO4:  D2
#define iPinBeep 0        // GPIO0:  D3
#define iPinLED_Motion 2  // GPIO2:  D4: Built in LED
#define iPinDHT 14        // GPIO14: D5
#define iPinLightD1 12    // GPIO12: D6
#define iPinLightD2 13    // GPIO13: D7
#define iPinLED_Door 15   // GPIO15: D8


// Wiring Info-----------------
// Door     GND,            D1
// PIR      GND, Vin (+5V), D2
// Buzzer   GND,            D3
// DHT      GND, 3.3V,      D5
// Light1   GND, A0         D6
// Light2   GND, A0         D7


#define eTypeDHT DHT22  // Select DHT type


// Configure DHT
DHT_Unified dht(iPinDHT, eTypeDHT);


bool bBeep        = false;
bool bBeepEnabled = true;

bool bMotion       = false;
bool bMotionUpdate = false;

bool bDoorOpen        = false;
bool bDoorAlertUpdate = false;
bool bDoorAlert       = false;
bool bDoorAlertTrig   = false;

bool bDaylight         = false;
bool bLightAlert       = false;
bool bLightAlertTrig   = false;
bool bLightAlertUpdate = false;

bool bUpdate           = false;
bool bUpdateTrig       = false;
bool bUpdateTempCmpt   = false;
bool bUpdateHumCmpt    = false;
bool bUpdateLightsCmpt = false;

int CntDoorOpen = 0;

int CntLoops    = 0;
int CntWifiFail = 0;

int CntLightIntensity1 = 0;
int CntLightIntensity2 = 0;

int CntMotionTimer = 0;

float PctHumidity = 0.0F;
float T_DHT       = 0.0F;

unsigned long tReadLightsStart  = 0UL;
unsigned long tWiFiConnectStart = 0UL;

os_timer_t myTimer;


sensors_event_t EventTemperature;
sensors_event_t EventHumidity;


void setup() {
    Serial.begin(115200);

    dht.begin();

    pinMode(iPinDoor, INPUT_PULLUP);
    pinMode(iPinMotion, INPUT);

    pinMode(iPinLightD1, OUTPUT);
    pinMode(iPinLightD2, OUTPUT);
    pinMode(iPinBeep, OUTPUT);
    pinMode(iPinLED_Motion, OUTPUT);
    pinMode(iPinLED_Door, OUTPUT);

    os_timer_setfn(&myTimer, timerCallback, NULL);
    os_timer_arm(&myTimer, 1000, true);

    digitalWrite(iPinLED_Motion, true);  // set to off (low side drive)
    digitalWrite(iPinLED_Door, false);
}


//
//
// Periodic loop timer

void timerCallback(void* pArg) {  // timer1 interrupt 1Hz

    bool bDoorLED;
    bool bMotionLED;

    // CntLoopsPost = number of seconds before making a new post
    if (CntLoops < CntLoopPost) {
        CntLoops++;
    }

    // eDoorOpenCal = DIO state when door is open (depends on sensor type)
    if (digitalRead(iPinDoor) == eDoorOpenCal) {
        bDoorLED       = true;
        bDoorOpen      = true;

        // CntDoorOpenBeepDelay = number of seconds when door is open before beeping starts
        if (CntDoorOpen < CntDoorOpenBeepDelay) {
            // Count up until beep delay expires
            CntDoorOpen++;
        } else {
            // Door has been open longer than delay cal
            // cycle the audible alert
            bBeep = !bBeep;
        }
    } else {
        bBeep       = false;
        bDoorLED    = false;
        bDoorOpen   = false;
        CntDoorOpen = 0;
    }

    digitalWrite(iPinBeep, bBeep);
    digitalWrite(iPinLED_Door, bDoorLED);

    if (digitalRead(iPinMotion)) {
        bMotion        = true;
        bMotionLED     = true;
        CntMotionTimer = 0;
    } else {
        bMotionLED = false;
        // CntMotionDelay = seconds to latch motion detection
        if (CntMotionTimer < CntMotionDelay) {
            CntMotionTimer++;
        } else {
            bMotion = false;
        }
    }
    digitalWrite(iPinLED_Motion, !bMotionLED);  // set LED (low side drive)

    Serial.printf(" CntLoops = %d", CntLoops);
    Serial.printf(" CntLoopPost = %d", CntLoopPost);
    Serial.printf(" bUpdate = %d", bUpdate);

    Serial.printf(" bUpdateLightsCmpt = %d", bUpdateLightsCmpt);
    Serial.printf(" bUpdateTempCmpt = %d", bUpdateTempCmpt);
    Serial.printf(" bUpdateHumCmpt = %d\n", bUpdateHumCmpt);


    Serial.printf(" bLightAlert = %d", bLightAlert);
    Serial.printf(" bLightAlertUpdate = %d", bLightAlertUpdate);
    Serial.printf(" bDoorOpen = %d", bDoorOpen);
    Serial.printf(" bDoorAlertUpdate = %d", bDoorAlertUpdate);
    Serial.printf(" bMotion = %d", bMotion);
    Serial.printf(" bMotionUpdate = %d\n\n", bMotionUpdate);
}


//
//
// Main loop to evaluate the need to post an update and reset the values

void loop() {

    bool bUpdatePrev = bUpdate;

    bLightAlert =
            (!bDaylight && !bMotion &&
             (CntLightIntensity1 > CntLightOnThresh || CntLightIntensity2 > CntLightOnThresh));

    bLightAlertTrig = bLightAlert && !bLightAlertUpdate;


    bDoorAlert = !bMotion && bDoorOpen && (CntDoorOpen == CntDoorOpenBeepDelay);

    bDoorAlertTrig = bDoorAlert && !bDoorAlertUpdate;


    bUpdate =
            ((bLightAlertTrig) || (bDoorAlertTrig) || (bMotion != bMotionUpdate) ||
             (CntLoops >= CntLoopPost));

    bUpdateTrig = bUpdate && !bUpdatePrev;

    if (bUpdate || bLightAlert) {
        ReadLights();
    }


    if (bUpdate) {
        Read_DHT();
    }


    if (WiFi.status() != WL_CONNECTED) {
        ConnectToWiFi();
    }


    if (bUpdate && bUpdateLightsCmpt && bUpdateTempCmpt && bUpdateHumCmpt &&
        WiFi.status() == WL_CONNECTED) {

        UpdateHomeAlerts();
        UpdateSheets();

        bDoorAlertUpdate  = bDoorAlert;
        bMotionUpdate     = bMotion;
        bLightAlertUpdate = bLightAlert;
        CntLoops          = 0;
    }


    if (CntWifiFail > CntWifiFailThresh) {
        ESP.restart();
    }
    ArduinoOTA.handle();
}


//
//
// Function to read temperature and humidity from the DHT

void Read_DHT() {

    if (bUpdateTrig) {
        bUpdateTempCmpt = false;
        bUpdateHumCmpt  = false;
        T_DHT           = -99.0F;
        PctHumidity     = -99.0F;
        dht.temperature().getEvent(&EventTemperature);
        dht.humidity().getEvent(&EventHumidity);
    }

    if (!isnan(EventTemperature.temperature) && EventTemperature.temperature > -90.0) {
        T_DHT           = EventTemperature.temperature;
        bUpdateTempCmpt = true;
    }

    if (!isnan(EventHumidity.relative_humidity) && EventHumidity.relative_humidity > -90.0) {
        PctHumidity    = EventHumidity.relative_humidity;
        bUpdateHumCmpt = true;
    }
}


//
//
// Function to multiplex the light sensors and read analog voltages

void ReadLights() {
    // Multiplexed to analog input
    // Digital outputs used to control which sensor is reporting

    unsigned long dtReadLights;
    unsigned long run_time = millis();

    if (bUpdateTrig || (bLightAlert && bUpdateLightsCmpt)) {
        tReadLightsStart  = run_time;
        bUpdateLightsCmpt = false;
    }

    dtReadLights = run_time - tReadLightsStart;

    if (dtReadLights < 700) {
        digitalWrite(iPinLightD1, HIGH);
        digitalWrite(iPinLightD2, LOW);
        CntLightIntensity1 = analogRead(A0);
    } else if (dtReadLights < 1400) {
        digitalWrite(iPinLightD1, LOW);
        digitalWrite(iPinLightD2, HIGH);
        CntLightIntensity2 = analogRead(A0);
    } else {
        bUpdateLightsCmpt = true;
        digitalWrite(iPinLightD1, LOW);
        digitalWrite(iPinLightD2, LOW);
    }
}


//
//
// Connect to Wifi

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

    http.begin(wificlient, *strURL);
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
        int iEnd    = strMain->indexOf(",", iStart);
        int lenFind = (int)strFind.length();

        String return_str = strMain->substring(iStart + lenFind, iEnd);

        Serial.println(strFind + return_str);

        *return_val = (bool)return_str.toInt();
    }
}

void FindIntInString(String* strMain, String strFind, int* return_val) {

    int iStart = strMain->indexOf(strFind);

    if (iStart != -1) {
        int iEnd    = strMain->indexOf(",", iStart);
        int lenFind = (int)strFind.length();

        String return_str = strMain->substring(iStart + lenFind, iEnd);

        Serial.println(strFind + return_str);

        *return_val = return_str.toInt();
    }
}

void UpdateSheets() {

    String url_string;
    String strReturn;

    url_string = "/macros/s/" + sheet_id + "/exec?room_name=" + room_name +
            "&Door=" + String(bDoorOpen) + "&Temperature=" + String(T_DHT) +
            "&Humidity=" + String(PctHumidity) + "&Motion=" + String(bMotion) +
            "&Light1=" + String(CntLightIntensity1) + "&Light2=" + String(CntLightIntensity2) +
            "&LightAlert=" + String(bLightAlert) + "&DoorAlert=" + String(bDoorAlert) +
            "&LightAlertTrig=" + String(bLightAlertTrig) +
            "&DoorAlertTrig=" + String(bDoorAlertTrig) + "&Daylight=" + String(bDaylight) + "&";

    Serial.println(url_string);
    Serial.println();

    GetHTTPS_String(&url_string, &strReturn);

    FindBoolInString(&strReturn, "bBeepEnabled\":", &bBeepEnabled);
    FindBoolInString(&strReturn, "bDaylight\":", &bDaylight);

    FindIntInString(&strReturn, "CntDoorOpenBeepDelay\":", &CntDoorOpenBeepDelay);
    FindIntInString(&strReturn, "CntLightOnThresh\":", &CntLightOnThresh);
    FindIntInString(&strReturn, "CntLoopPost\":", &CntLoopPost);
    FindIntInString(&strReturn, "CntMotionDelay\":", &CntMotionDelay);
    FindIntInString(&strReturn, "CntWifiRetryAbort\":", &CntWifiRetryAbort);

    FindBoolInString(&strReturn, "eDoorOpenCal\":", &eDoorOpenCal);
}


//
//
// Update the home center with the collected data

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
