
#include <DHT.h>
#include "ScriptConfig.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

extern "C" {
#include "user_interface.h"
}


// TODO: Update IFTTT for door state change and lights on / off at night
// TODO: Use IFTTT to update a google sheet with the data


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

#if false

// Basement
String strRoom = "Basement";
#define eDoorOpenCal true

#else
// Garage
String strRoom = "Garage";
#define eDoorOpenCal false

#endif

// Configure DHT
DHT dht(iPinDHT, eTypeDHT);


bool bBeep = false;

bool bMotion     = false;
bool bMotionPrev = false;

bool bDoorOpen     = false;
bool bDoorOpenPrev = false;

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
        bDoorLED  = true;
        bDoorOpen = true;

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

    if ((bLightsTurnedOff) || (bDoorOpen != bDoorOpenPrev) || (bMotion != bMotionPrev) ||
        (CntLoops >= CntLoopPost)) {
        bUpdate = true;
    }

    bDoorOpenPrev = bDoorOpen;
    bMotionPrev   = bMotion;

    if (bUpdate) {

        Read_DHT();
        if (bReadLights) {
            ReadLights();
        }

        ConnectToWiFi();

        if (WiFi.status() == WL_CONNECTED) {

            UpdateHomeCenter();
            bUpdate  = false;
            CntLoops = 0;
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


void GetHTTP_String(String* strURL, String* strReturn) {

    HTTPClient http;

    http.begin(*strURL);
    int httpCode = http.GET();
    Serial.println("httpCode=" + String(httpCode));

    if (httpCode > 0) {
        *strReturn = http.getString();
        Serial.println(*strReturn);
    }

    http.end();
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

    String strReturn;
    GetHTTP_String(&s, &strReturn);

    Serial.println(s);
    Serial.println("strReturn:");
    Serial.println(strReturn);

    FindBoolInString(strReturn, "bDaylight\":\"", &bDaylight);
    FindIntInString(strReturn, "CntLightOnThresh\":\"", &CntLightOnThresh);
}