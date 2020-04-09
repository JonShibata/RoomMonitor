
#include <DHT.h>
#include "ScriptConfig.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

extern "C" {
#include "user_interface.h"
}


// TODO: Update IFTTT for door state change and lights on / off at night


// Define pin locations

#define iPinDoor 5        // GPIO5:  D1
#define iPinMotion 4      // GPIO4:  D2
#define iPinBeep 0        // GPIO0:  D3
#define iPinLED_Motion 2  // GPIO2:  D4: Built in LED
#define iPinDHT 14        // GPIO14: D5
#define iPinLightD1 12    // GPIO12: D6
#define iPinLightD2 13    // GPIO13: D7
#define iPinLED_Door 15   // GPIO15: D8

#define eTypeDHT DHT22  // Select DHT type


// Configure DHT
DHT dht(iPinDHT, eTypeDHT);


bool bBeep = false;

bool bMotion       = false;
bool bMotionUpdate = false;

bool bDoorOpen       = false;
bool bDoorOpenLatch  = false;
bool bDoorOpenUpdate = false;

bool bDaylight = false;

bool bUpdate = true;

int CntDoorOpen = 0;

int CntLoops = 0;

int CntLightIntensity1     = 0;
int CntLightIntensity1Prev = 0;
int CntLightIntensity2     = 0;
int CntLightIntensity2Prev = 0;

int CntMotionTimer = 0;

float PctHumidity = 0.0F;
float T_DHT       = 0.0F;

float tSunrise;
float tSunset;

unsigned long ulTime = ULONG_LONG_MAX;

os_timer_t myTimer;

//
//
// declare reset function at address 0
void (*resetFunc)(void) = 0;

//
//
// Setup function called on boot
void setup() {

    Serial.begin(115200);

    pinMode(iPinDoor, INPUT_PULLUP);
    pinMode(iPinMotion, INPUT);
    pinMode(iPinDHT, INPUT_PULLUP);

    pinMode(iPinLightD1, OUTPUT);
    pinMode(iPinLightD2, OUTPUT);
    pinMode(iPinBeep, OUTPUT);
    pinMode(iPinLED_Motion, OUTPUT);
    pinMode(iPinLED_Door, OUTPUT);

    os_timer_setfn(&myTimer, timerCallback, NULL);
    os_timer_arm(&myTimer, 1000, true);

    digitalWrite(iPinLED_Motion, true);  // set to off (low side drive)
    digitalWrite(iPinLED_Door, false);

    ConnectToWiFi();
}

//
//
// Periodic loop timer

void timerCallback(void* pArg) {  // timer1 interrupt 1Hz

    bool bDoorLED;
    bool bMotionLED;

    if (CntLoops < CntLoopPost) {
        CntLoops++;
    }

    if (digitalRead(iPinDoor) == eDoorOpenCal) {
        bDoorLED       = true;
        bDoorOpen      = true;
        bDoorOpenLatch = true;

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
        if (CntMotionTimer < CntMotionDelay) {
            CntMotionTimer++;
        } else {
            bMotion = false;
        }
    }
    digitalWrite(iPinLED_Motion, !bMotionLED);  // set LED (low side drive)

    Serial.print(" bDoorOpen = " + String(bDoorOpen));
    Serial.print(" bBeep = " + String(bBeep));
    Serial.println(" CntLoops = " + String(CntLoops));
}


//
//
// Main loop to evaluate the need to post an update and reset the values

void loop() {
    bool bReadLights      = true;
    bool bLightsTurnedOff = false;

    if (!bDaylight) {
        if (CntLightIntensity1Prev > CntLightOnThresh ||
            CntLightIntensity2Prev > CntLightOnThresh) {
            ReadLights();
            bReadLights = false;
            if (CntLightIntensity1 < CntLightOnThresh && CntLightIntensity2 < CntLightOnThresh)
                bLightsTurnedOff = true;
        }
    }

    if ((bLightsTurnedOff) || (bDoorOpenLatch != bDoorOpenUpdate) || (bMotion != bMotionUpdate) ||
        (CntLoops >= CntLoopPost)) {
        bUpdate = true;
    }


    if (bUpdate) {

        Read_DHT();
        if (bReadLights) {
            ReadLights();
        }

        if (WiFi.status() != WL_CONNECTED) {
            ConnectToWiFi();
        }

        if (WiFi.status() == WL_CONNECTED) {
            DetermineDaylight();
            UpdateSheets();
            UpdateHomeCenter();
            bUpdate         = false;
            bDoorOpenUpdate = bDoorOpenLatch;
            bDoorOpenLatch  = false;
            bMotionUpdate   = bMotion;
            CntLoops        = 0;
        }
    }
}


//
//
// Function to read temperature and humidity from the DHT

void Read_DHT() {
    int CntTempReads     = 0;
    int CntHumidityReads = 0;

    T_DHT = -99.0F;
    while ((isnan(T_DHT) || T_DHT < -90.0F) && (CntTempReads < 5)) {
        // Read temperature as Fahrenheit (isFahrenheit = true)
        T_DHT = dht.readTemperature(true);
        delay(500);
        CntTempReads++;
    }

    if (isnan(T_DHT) || T_DHT < -90.0F) {
        T_DHT = 0.0F;
    }

    Serial.println(" T_DHT       = " + String(T_DHT));

    PctHumidity = -99.0F;
    while ((isnan(PctHumidity) || PctHumidity < -90.0F) && (CntHumidityReads < 5)) {
        // Reading temperature or humidity takes about 250 ms
        PctHumidity = dht.readHumidity();
        delay(500);
        CntHumidityReads++;
    }

    if (isnan(PctHumidity) || PctHumidity < -90.0F) {
        PctHumidity = 0.0F;
    }

    Serial.println(" PctHumidity = " + String(PctHumidity));
}


//
//
// Function to multiplex the light sensors and read analog voltages

void ReadLights() {
    // Multiplexed to analog input
    // Digital outputs used to control which sensor is reporting
    digitalWrite(iPinLightD1, HIGH);
    delay(500);

    CntLightIntensity1 = analogRead(A0);
    delay(200);

    digitalWrite(iPinLightD1, LOW);
    digitalWrite(iPinLightD2, HIGH);
    delay(500);

    CntLightIntensity2 = analogRead(A0);
    delay(200);

    digitalWrite(iPinLightD2, LOW);

    CntLightIntensity1Prev = CntLightIntensity1;
    CntLightIntensity2Prev = CntLightIntensity2;

    Serial.println(" Light1      = " + String(CntLightIntensity1));
    Serial.println(" Light2      = " + String(CntLightIntensity2));
}


//
//
// Connect to Wifi

void ConnectToWiFi() {
    int CntWifiRetries = 0;
    int intWiFiCode;

    WiFi.mode(WIFI_STA);
    intWiFiCode = WiFi.begin(ssid, password);

    Serial.println("");
    Serial.println("Connecting to WiFi");
    Serial.println("");
    Serial.println("WiFi.begin = " + String(intWiFiCode));

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
    }
}


void DetermineDaylight() {
    String strSunrise, strSunset, strDateTime;
    // String* ptrUpdateTime;

    bool bUpdateSunriseSunset = false;
    // if millis has wrapped around or if 1 day has past then update
    if (millis() < 8640000) {
        if (ulTime > 86400000) {
            bUpdateSunriseSunset = true;
        }
    } else {
        if (ulTime < millis() - 86400000) {
            bUpdateSunriseSunset = true;
        }
    }


    if (bUpdateSunriseSunset) {

        String strRequest =
                "http://api.sunrise-sunset.org/json?lat=42.391&lng=-83.779&formatted=0";
        String strReturn;
        int    httpCode = GetHTTP_String(&strRequest, &strReturn);

        if (httpCode > 0) {
            ulTime   = millis();
            tSunrise = GetDate_Hour_Min(strReturn, "civil_twilight_begin", &strSunrise);
            tSunset  = GetDate_Hour_Min(strReturn, "civil_twilight_end", &strSunset);
        }
    }


    String strRequest = "http://worldtimeapi.org/api/timezone/America/Detroit";
    String strReturn;
    int    httpCode = GetHTTP_String(&strRequest, &strReturn);

    if (httpCode > 0) {
        String strSearch = "utc_datetime";

        float tNow = GetDate_Hour_Min(strReturn, strSearch, &strDateTime);

        if (tSunset > tSunrise) {
            bDaylight = (tSunrise < tNow) && (tNow < tSunset);
        } else {
            bDaylight = (tSunrise < tNow) || (tNow < tSunset);
        }
    }
}

float GetDate_Hour_Min(String strMain, String strSearch, String* strDateTime) {

    int lenSearchStr = strSearch.length() + 3;  // 3 for ":"

    int iStart   = strMain.indexOf(strSearch) + lenSearchStr;
    *strDateTime = strMain.substring(iStart, iStart + 19);
    Serial.println(strSearch + " - " + *strDateTime);

    float fltHoursTemp   = strMain.substring(iStart + 11, iStart + 13).toFloat();
    float fltMinutesTemp = strMain.substring(iStart + 14, iStart + 16).toFloat();

    return (fltHoursTemp + (fltMinutesTemp / 60.0F));
}

int GetHTTP_String(String* strURL, String* strReturn) {

    HTTPClient http;

    http.begin(*strURL);
    int httpCode = http.GET();
    Serial.println("httpCode=" + String(httpCode));

    if (httpCode > 0) {
        *strReturn = http.getString();
        Serial.println(*strReturn);
    }

    http.end();
    return httpCode;
}


void FindIntInString(String strMain, String strFind, int* ptrData) {
    int iStart = strMain.indexOf(strFind);
    if (iStart != -1) {
        int iEnd    = strMain.indexOf(",", iStart);
        int lenFind = (int)strFind.length();
        *ptrData    = strMain.substring(iStart + lenFind, iEnd).toInt();
        Serial.println(strFind + String(*ptrData));
    }
}


void FindBoolInString(String strMain, String strFind, bool* ptrData) {
    int iStart = strMain.indexOf(strFind);
    if (iStart != -1) {
        int lenFind = (int)strFind.length();
        int iData   = iStart + lenFind;
        *ptrData    = (bool)strMain.substring(iData, iData + 1).toInt();
        Serial.println(strFind + String(*ptrData));
    }
}


void UpdateSheets() {
    String s;
    String strReturn;

    s = ifttt_server + ifttt_action1 + strCellLight1 + String(CntLightIntensity1);
    GetHTTP_String(&s, &strReturn);
    s = ifttt_server + ifttt_action1 + strCellLight2 + String(CntLightIntensity2);
    GetHTTP_String(&s, &strReturn);
    s = ifttt_server + ifttt_action1 + strCellDoor + String(bDoorOpenLatch);
    GetHTTP_String(&s, &strReturn);
    s = ifttt_server + ifttt_action1 + strCellMotion + String(bMotion);
    GetHTTP_String(&s, &strReturn);
    s = ifttt_server + ifttt_action1 + strCellTemp + String(T_DHT);
    GetHTTP_String(&s, &strReturn);
    s = ifttt_server + ifttt_action1 + strCellHum + String(PctHumidity);
    GetHTTP_String(&s, &strReturn);

    s = ifttt_server + ifttt_action2 + "?value1=" + String(bMotion) +
            "&value2=" + String(CntLightIntensity1) + "&value3=" + String(CntLightIntensity2);
    GetHTTP_String(&s, &strReturn);

    s = ifttt_server + ifttt_action3 + "?value1=" + String(bDoorOpenLatch) +
            "&value2=" + String(T_DHT) + "&value3=" + String(PctHumidity);
    GetHTTP_String(&s, &strReturn);
}


//
//
// Update the home center with the collected data

void UpdateHomeCenter() {

    String s = "http://" + host;
    s += "/bHomeMonitor=1&strRoom=" + strRoom + "&";
    s += "bDoorOpen=" + String(bDoorOpen) + "&";
    s += "bMotion=" + String(bMotion) + "&";
    s += "CntLightIntensity1=" + String(CntLightIntensity1) + "&";
    s += "CntLightIntensity2=" + String(CntLightIntensity2) + "&";
    s += "T_DHT=" + String(T_DHT) + "&";
    s += "PctHumidity=" + String(PctHumidity) + "&";
    s += "bDaylight=" + String(bDaylight) + "&";

    String strReturn;
    GetHTTP_String(&s, &strReturn);

    Serial.println(s);
    Serial.println("strReturn:");
    Serial.println(strReturn);

    FindIntInString(strReturn, "CntDoorOpenBeepDelay\":\"", &CntDoorOpenBeepDelay);
    FindIntInString(strReturn, "CntLightOnThresh\":\"", &CntLightOnThresh);
    FindIntInString(strReturn, "CntMotionDelay\":\"", &CntMotionDelay);
    FindIntInString(strReturn, "CntWifiRetryAbort\":\"", &CntWifiRetryAbort);
}