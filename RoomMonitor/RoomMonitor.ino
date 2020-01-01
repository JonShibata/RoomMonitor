
#include <DHT.h>
#include "Network_Info.h"
#include <ESP8266WiFi.h>

extern "C"
{
#include "user_interface.h"
}

// Define pin locations

#define iPinDoor 5       // GPIO5:  D1
#define iPinMotion 4     // GPIO4:  D2
#define iPinBeep 0       // GPIO0:  D3
#define iPinLED_Motion 2 // GPIO2:  D4: Built in LED
#define iPinDHT 14       // GPIO14: D5
#define iPinLightD1 12   // GPIO12: D6
#define iPinLightD2 13   // GPIO13: D7
#define iPinLED_Door 15  // GPIO15: D8

#define bLightOn false // LED low side drive
#define bLightOff true // LED low side drive

#define eTypeDHT DHT22 // Select DHT type

#if true

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

// Script Calibrations
const int CntLoopPost = 3600;     // = 3600; // (seconds) 60 min * 60 sec/min
const int CntDoorBeepThresh = 10; // (seconds)
const int tMotionDelay = 300;     // Secs to set update when motion detected

int CntLightOnThresh = 50; // Light threshold to determine light is on

bool bBeep = false;

bool bMotion = false;

bool bDoorOpen = false;
bool bDoorOpenPrev = false;

bool bDaylight = false;

bool bUpdate = true;

int CntDoorOpenBeep = 0;

int CntLoops = 0;

int CntLightIntensity1 = 0;
int CntLightIntensity1Prev = 0;
int CntLightIntensity2 = 0;
int CntLightIntensity2Prev = 0;

int tMotionTimer = 0;
int tMotionTimerPrev = 0;

float PctHumidity = 0.0F;
float T_DHT = 0.0F;

os_timer_t myTimer;

//
//
// declare reset function at address 0
void (*resetFunc)(void) = 0;

//
//
// Setup function called on boot
void setup()
{

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
    digitalWrite(iPinLED_Door, false);
}

//
//
// Periodic loop timer

void timerCallback(void *pArg)
{ // timer1 interrupt 1Hz

    bool bDoorLED;
    bool bMotionLED;

    if (CntLoops < 32400)
    {

        CntLoops++;
    }

    if (digitalRead(iPinDoor) == eDoorOpenCal)
    {
        bDoorLED = true;
        bDoorOpen = true;

        if (CntDoorOpenBeep < CntDoorBeepThresh)
        {
            // Coumt up until beep delay expires
            CntDoorOpenBeep++;
        }
        else
        {
            // Door has been open longer than threshold
            // cycle the audible alert
            bBeep = !bBeep;
        }
    }
    else
    {
        bBeep = false;
        bDoorLED = false;
        bDoorOpen = false;
        CntDoorOpenBeep = 0;
    }

    digitalWrite(iPinBeep, bBeep);
    digitalWrite(iPinLED_Door, bDoorLED);

    tMotionTimerPrev = tMotionTimer;
    if (digitalRead(iPinMotion))
    {
        bMotionLED = bLightOn;
        tMotionTimer = 0;
    }
    else
    {
        bMotionLED = bLightOff;
        if (tMotionTimer < tMotionDelay)
        {
            tMotionTimer++;
        }
    }
    digitalWrite(iPinLED_Motion, bMotionLED);

    // Serial.print(" bDoorOpen = " + String(bDoorOpen));
    // Serial.print(" bBeep = " + String(bBeep));
    // Serial.print(" tMotionTimer = " + String(tMotionTimer));
    // Serial.println(" CntLoops = " + String(CntLoops));
}

//
//
// Main loop to evaluate the need to post an update and reset the values

void loop()
{
    bool bReadLights = true;
    bool bLightsTurnedOff = false;

    if (!bDaylight)
    {
        if (CntLightIntensity1Prev > CntLightOnThresh ||
            CntLightIntensity2Prev > CntLightOnThresh)
        {
            ReadLights();
            bReadLights = false;
            if (CntLightIntensity1 < CntLightOnThresh &&
                CntLightIntensity2 < CntLightOnThresh)
                bLightsTurnedOff = true;
        }
    }

    if ((bLightsTurnedOff) ||
        (bDoorOpen != bDoorOpenPrev) ||
        (tMotionTimerPrev >= tMotionDelay && tMotionTimer < tMotionDelay) || // timer started
        (tMotionTimerPrev < tMotionDelay && tMotionTimer >= tMotionDelay) || // timer complete
        (CntLoops >= CntLoopPost))
    {
        bUpdate = true;
    }

    if (bUpdate)
    {

        Read_DHT();
        if (bReadLights)
        {
            ReadLights();
        }

        ConnectToWiFi();

        if (WiFi.status() == WL_CONNECTED)
        {
            UpdateHomeCenter();
            bUpdate = false;
            CntLoops = 0;
        }
    }

    bDoorOpenPrev = bDoorOpen;
}

//
//
// Function to read temperature and humidity from the DHT

void Read_DHT()
{
    int CntTempReads = 0;
    int CntHumidityReads = 0;

    T_DHT = -99.0F;
    while ((isnan(T_DHT) || T_DHT < -90.0F) && (CntTempReads < 5))
    {
        // Read temperature as Fahrenheit (isFahrenheit = true)
        T_DHT = dht.readTemperature(true);
        delay(500);
        CntTempReads++;
    }

    if (isnan(T_DHT) || T_DHT < -90.0F)
    {
        T_DHT = 0.0F;
    }

    Serial.println(" T_DHT       = " + String(T_DHT));

    PctHumidity = -99.0F;
    while ((isnan(PctHumidity) || PctHumidity < -90.0F) && (CntHumidityReads < 5))
    {
        // Reading temperature or humidity takes about 250 ms
        PctHumidity = dht.readHumidity();
        delay(500);
        CntHumidityReads++;
    }

    if (isnan(PctHumidity) || PctHumidity < -90.0F)
    {
        PctHumidity = 0.0F;
    }

    Serial.println(" PctHumidity = " + String(PctHumidity));
}

//
//
// Function to multiplex the light sensors and read analog voltages

void ReadLights()
{
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

void ConnectToWiFi()
{
    int CntWifiRetries = 0;
    int intWiFiCode;

    WiFi.mode(WIFI_STA);
    intWiFiCode = WiFi.begin(ssid, password);

    Serial.println("");
    Serial.println("Connecting to WiFi");
    Serial.println("");
    Serial.println("WiFi.begin = " + String(intWiFiCode));

    while ((WiFi.status() != WL_CONNECTED) && (CntWifiRetries < CntWifiRetryAbort))
    {
        CntWifiRetries++;
        delay(1000);
        Serial.print(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("");
        Serial.println("WiFi NOT Connected");
    }
    else
    {
        Serial.println("");
        Serial.println("WiFi Connected");
    }
}

//
//
// Update the home center with the collected data

void UpdateHomeCenter()
{

    // TODO: Adjust the post strings to reflect the updated data and update triggers

    String strLight1;
    String strLight2;
    String strDoorOpen;
    String strDaylight;
    String strMotion;
    String strT_DHT;
    String strPctHumidity;

    const int httpPort = 80;

    WiFiClient client;

    if (!client.connect(host, httpPort))
    {
        Serial.println("connection failed");
        return;
    }

    if (bDoorOpen)
    {
        strDoorOpen = "/bDoorOpen" + strRoom + "=1;";
    }
    else
    {
        strDoorOpen = "/bDoorOpen" + strRoom + "=0;";
    }

    if (tMotionTimer > 0)
    {
        strMotion = "/bMotion" + strRoom + "=1;";
    }
    else
    {
        strMotion = "/bMotion" + strRoom + "=0;";
    }

    strLight1 =
        "/CntLightIntensity1" + strRoom + "=" + String(CntLightIntensity1) + ";";

    strLight2 =
        "/CntLightIntensity2" + strRoom + "=" + String(CntLightIntensity2) + ";";

    // Send request to the home center
    String strUrl =
        "GET " + strDoorOpen + strMotion + strLight1 + strLight2 +
        "T_DHT" + strRoom + "=" + String(T_DHT) + ";" +
        "PctHumidity" + strRoom + "=" + String(PctHumidity) + ";" +
        " HTTP/1.1\r\n" + "Host: " + host + "\r\n" +
        "Connection: close\r\n\r\n";

    Serial.println(strUrl);
    client.print(strUrl);

    // Read all the lines of the reply from server and print them to Serial
    while (client.available())
    {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }
}