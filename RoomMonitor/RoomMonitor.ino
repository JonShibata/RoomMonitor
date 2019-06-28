
#include "DHT.h"
#include "Network_Info.h"
#include "ThingSpeak.h"
#include "ThingSpeakConfig.h"
#include <ESP8266WiFi.h>

extern "C" {
#include "user_interface.h"
}

// Define pin locations

#define iPinDoor 5        // GPIO5:  D1
#define iPinMotion 4      // GPIO4:  D2
#define iPinBeep 0        // GPIO0:  D3
#define iPinLED_Motion 2  // GPIO2:  D4: Built in LED
#define iPinDHT 14        // GPIO14: D5
#define iPinLightD1 12    // GPIO12: D6
#define iPinLightD2 13    // GPIO13: D7
#define iPinLED_Door 15   // GPIO15: D8

#define bLightOn false  // LED low side drive
#define bLightOff true  // LED low side drive

#define eTypeDHT DHT22  // Select DHT type

#if true
// Basement
#define DataChannel BasementChannel
#define DataWriteKey BasementWriteKey
String strRoom = "Basement";
#define bDoorOpenCal true

#else
// Garage
#define DataChannel GarageChannel
#define DataWriteKey GarageWriteKey
String strRoom = "Garage";
#define bDoorOpenCal false
#endif

// Configure DHT
DHT dht(iPinDHT, eTypeDHT);

// Script Calibrations

const int CntLoopFast = 300;        // = 300;  // (seconds)  5 min * 60 sec/min
const int CntLoopSlow = 1800;       // = 1800; // (seconds) 30 min * 60 sec/min
const int CntWifiRetryAbort = 240;  // = 240;   // (seconds) 4 min * 60 sec/min
const int CntDoorBeepThresh = 10;   // (seconds)
const int CntMotionThresh = 5;      // Motion events to trigger fast posts
const int CntLightOnThresh = 50;    // Light threshold to determine light is on
const int CntUpdateFailThresh = 5;  // Update fail threshold to try a reset

const float PctMotionThresh = 2.0F;  // Pct of time motion detected below which room is empty

bool bBeep = false;
bool bDaylight = false;
bool bDoorOpen = false;
bool bDoorOpenLatch = false;
bool bDoorOpenLatchPrev = false;
bool bFirstPostComplete = false;

int CntDoorOpen = 0;
int CntDoorOpenBeep = 0;
int CntLoops = 0;
int CntMotionEvents = 0;
int CntLightIntensity1 = 0;
int CntLightIntensity2 = 0;
int CntUpdateFails = 0;
int intThingSpeakCode = 0;
int tSunOfst = 0;

int CntLoopPost = CntLoopSlow;

float PctHumidity = 0.0F;
float PctMotion = 0.0F;
float tSunRise = 0.0F;
float tSunSet = 0.0F;
float T_DHT = 0.0F;

String strHours = "";
String strMinutes = "";
String tPostStr = "";

float Data[8];

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

    digitalWrite(iPinLED_Motion, bLightOff);
    digitalWrite(iPinLED_Door, bLightOff);
}

//
//
// Periodic loop timer

void timerCallback(void* pArg) {  // timer1 interrupt 1Hz

    CntLoops++;
    CntLoops = min(CntLoops, 32400);

    if (digitalRead(iPinDoor) == bDoorOpenCal) {
        digitalWrite(iPinLED_Door, bLightOn);
        bDoorOpen = true;
        bDoorOpenLatch = true;

        CntDoorOpen++;
        CntDoorOpen = min(CntDoorOpen, 32400);
        CntDoorOpenBeep++;
        CntDoorOpenBeep = min(CntDoorOpenBeep, 32400);

        if (CntDoorOpenBeep > CntDoorBeepThresh) {
            // Door has been open longer than threshold
            // cycle the audible alert
            CntDoorOpenBeep = CntDoorBeepThresh;
            bBeep = !bBeep;
            digitalWrite(iPinBeep, bBeep);
        }
    } else {
        digitalWrite(iPinLED_Door, bLightOff);
        bBeep = false;
        bDoorOpen = false;
        CntDoorOpenBeep = 0;
        digitalWrite(iPinBeep, bBeep);
    }

    if (digitalRead(iPinMotion)) {
        CntMotionEvents++;
        CntMotionEvents = min(CntMotionEvents, 32400);

        digitalWrite(iPinLED_Motion, bLightOn);
    } else {
        digitalWrite(iPinLED_Motion, bLightOff);
    }
    Serial.print(" bDoorOpen = " + String(bDoorOpen));
    Serial.print(" bBeep = " + String(bBeep));
    Serial.print(" CntMotionEvents = " + String(CntMotionEvents));
    Serial.println(" CntLoops = " + String(CntLoops));
}

//
//
// Main loop to evaluate the need to post an update and reset the values

void loop() {

    if (CntLoops > 10) {
        PctMotion = ((float)CntMotionEvents / (float)CntLoops) * 100.0F;
    } else {
        PctMotion = 0;
    }

    if (PctMotion > PctMotionThresh || bDoorOpenLatch || bDoorOpenLatchPrev
        || ((!bDaylight)
            && (CntLightIntensity1 > CntLightOnThresh || CntLightIntensity2 > CntLightOnThresh))) {
        CntLoopPost = CntLoopFast;
    }

    if (((CntLoops >= CntLoopPost) || (!bFirstPostComplete)) && CntLoops > 10) {

        Serial.println("");
        Serial.println("Calculate Data 0-6");

        Read_DHT();
        ReadLights();

        Data[0] = (float)bDoorOpen;
        Data[1] = ((float)CntDoorOpen / (float)CntLoops) * 100.0F;
        Data[2] = (PctMotion);
        Data[3] = (float)T_DHT;
        Data[4] = (float)PctHumidity;
        Data[5] = (float)CntLightIntensity1;
        Data[6] = (float)CntLightIntensity2;

        bDoorOpenLatchPrev = bDoorOpenLatch;
        bDoorOpenLatch = false;
        bFirstPostComplete = true;

        CntDoorOpen = 0;
        CntLoops = 0;
        CntLoopPost = CntLoopSlow;
        CntMotionEvents = 0;

        ConnectToWiFi();

        if (WiFi.status() == WL_CONNECTED) {
            PostToThingspeakFunc();

            UpdateHomeCenter();

            Serial.println("");
            Serial.println("WiFi.disconnect = " + String((int)WiFi.disconnect()));
        }
        if (intThingSpeakCode != 200) {
            CntUpdateFails++;
            if (CntUpdateFails > CntUpdateFailThresh) {
                // Reset the board if too many attempts to connect to web fail
                resetFunc();
            }
        }
    }
}

//
//
// Function to read temperature and humidity from the DHT

void Read_DHT() {
    int CntTempReads = 0;
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


// Function to post measured data to ThingSpeak

void PostToThingspeakFunc() {

    int CntThingSpeakRetries = 0;
    int intCode1 = -999;
    int intCode2 = -999;
    int intCode3 = -999;
    String strTemp1 = String();
    String strTemp2 = String();
    String strTemp3 = String();


    Serial.println("");
    Serial.println("Begin Post");

    WiFiClient client;
    ThingSpeak.begin(client);

    for (int iData = 0; iData < 8; iData++) {
        ThingSpeak.setField(iData + 1, Data[iData]);
    }

    intThingSpeakCode = -999;

    while (intThingSpeakCode != 200 && CntThingSpeakRetries < 5) {
        ThingSpeak.setField(8, (float)intThingSpeakCode);
        intThingSpeakCode = ThingSpeak.writeFields(DataChannel, DataWriteKey);
        tPostStr = ThingSpeak.readCreatedAt(DataChannel, DataWriteKey);

        strTemp1 = ThingSpeak.readStringField(SunChannel, 1, SunReadKey);
        intCode1 = ThingSpeak.getLastReadStatus();

        strTemp2 = ThingSpeak.readStringField(SunChannel, 2, SunReadKey);
        intCode2 = ThingSpeak.getLastReadStatus();

        strTemp3 = ThingSpeak.readStringField(SunChannel, 3, SunReadKey);
        intCode3 = ThingSpeak.getLastReadStatus();

        // Serial.println("tSunRise   = " + strTemp1);
        // Serial.println("tSunSet    = " + strTemp2);
        // Serial.println("tSunOfst   = " + strTemp3);


        int index_start;
        int index_end;
        String str_temp;

        index_start = strTemp1.indexOf('\n');
        index_end = strTemp1.indexOf('\n', index_start + 1);
        str_temp = strTemp1.substring(index_start + 1, index_end - 1);
        // Serial.println("start   = " + String(index_start));
        // Serial.println("end     = " + String(index_end));
        // Serial.println("new tSunRise   = " + str_temp);
        tSunRise = str_temp.toFloat();

        index_start = strTemp2.indexOf('\n');
        index_end = strTemp2.indexOf('\n', index_start + 1);
        str_temp = strTemp2.substring(index_start + 1, index_end - 1);
        // Serial.println("start   = " + String(index_start));
        // Serial.println("end     = " + String(index_end));
        // Serial.println("new tSunSet   = " + str_temp);
        tSunSet = str_temp.toFloat();

        index_start = strTemp3.indexOf('\n');
        index_end = strTemp3.indexOf('\n', index_start + 1);
        str_temp = strTemp3.substring(index_start + 1, index_end - 1);
        // Serial.println("start   = " + String(index_start));
        // Serial.println("end     = " + String(index_end));
        // Serial.println("new tSunSet   = " + str_temp);
        tSunOfst = str_temp.toFloat();

        Serial.println("WriteFields = " + String(intThingSpeakCode));
        Serial.println("read code1  = " + String(intCode1));
        Serial.println("read code2  = " + String(intCode2));
        Serial.println("read code3  = " + String(intCode3));

        CntThingSpeakRetries++;
        if (intThingSpeakCode != 200) {
            delay(15000);
        }
    }

    if (intThingSpeakCode == 200 && intCode1 == 200 && intCode2 == 200 && intCode3 == 200) {
        float Hours;
        float Minutes;
        float tPostFloat;

        // Serial.println(tPostStr);

        strHours = tPostStr.substring(11, 13);
        strMinutes = tPostStr.substring(14, 16);

        Hours = strHours.toFloat();
        Minutes = strMinutes.toFloat();

        Serial.println("HoursRaw   = " + strHours);
        Serial.println("MinutesRaw = " + strMinutes);

        Hours += (float)tSunOfst;

        if (Hours < 0.0F) {
            Hours += 24.0F;
        }

        if (Hours < 10.0F) {
            strHours = '0' + String((int)Hours);
        } else {
            strHours = String((int)Hours);
        }

        Serial.println("Hours   = " + strHours);
        Serial.println("Minutes = " + String(Minutes));

        tPostFloat = (Hours + (Minutes / 60.0F));
        Serial.println("tPostFloat = " + String(tPostFloat));
        Serial.println("tSunrise   = " + String(tSunRise));
        Serial.println("tSunset    = " + String(tSunSet));
        Serial.println("tSunOfst   = " + String(tSunOfst));

        bDaylight = (tSunRise < tPostFloat && tPostFloat < tSunSet);
        Serial.println("bDaylight  = " + String(bDaylight));
    }

    Serial.println("");
    Serial.println("Post to Thingspeak End");
}

//
//
// Update the home center with the collected data

void UpdateHomeCenter() {
    String strLightOn;
    String strDoorOpen;
    String strDaylight;
    String strMotion;

    const int httpPort = 80;

    WiFiClient client;

    if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return;
    }

    if (bDoorOpen) {
        strDoorOpen = "/b" + strRoom + "DoorOpen=1";
    } else {
        strDoorOpen = "/b" + strRoom + "DoorOpen=0";
    }

    if (CntLightIntensity1 > CntLightOnThresh || CntLightIntensity2 > CntLightOnThresh) {
        strLightOn = "/b" + strRoom + "LightOn=1";
    } else {
        strLightOn = "/b" + strRoom + "LightOn=0";
    }

    if (PctMotion > PctMotionThresh) {
        strMotion = "/b" + strRoom + "Motion=1";
    } else {
        strMotion = "/b" + strRoom + "Motion=0";
    }

    if (bDaylight) {
        strDaylight = "/bDaylight=1";
    } else {
        strDaylight = "/bDaylight=0";
    }

    // Send request to the home center
    String strUrl = "GET " + strLightOn + strDoorOpen + strDaylight + strMotion
            + "/Hours=" + strHours + "/Minutes=" + strMinutes + " HTTP/1.1\r\n" + "Host: " + host
            + "\r\n" + "Connection: close\r\n\r\n";

    Serial.println(strUrl);
    client.print(strUrl);

    // Read all the lines of the reply from server and print them to Serial
    while (client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }
}