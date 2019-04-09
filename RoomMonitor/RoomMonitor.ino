

#include "DHT.h"
#include <ESP8266WiFi.h>
#include "ThingSpeak.h"
#include "ThingSpeakConfig.h"
#include "Network_Info.h"

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

const int CntLoopFast = 300;       // = 300;  // (seconds)  5 min * 60 sec/min
const int CntLoopSlow = 1800;      // = 1800; // (seconds) 30 min * 60 sec/min
const int CntWifiRetryAbort = 240; // = 240;   // (seconds) 4 min * 60 sec/min
const int CntDoorBeepThresh = 10;  // (seconds)
const int CntMotionThresh = 5;     // Motion events to trigger fast posts
const int CntLightOnThresh = 40;   // Light threshold to determine light is on
                                   // Check this value against thingspeak cals

bool bBeep;
bool bDoorOpen;
bool bDoorOpenLatch;
bool bDoorOpenLatchPrev;
int CntDoorOpen;
int CntDoorOpenBeep;
int CntLoops;
int CntMotionEvents;

int CntLoopPost = CntLoopSlow;

float T_DHT;
float PctHumidity;

int CntLightIntensity1;
int CntLightIntensity2;

float Data[8];

os_timer_t myTimer;

WiFiClient client;

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
  digitalWrite(iPinLED_Door, bLightOff);
}

//
//
// Periodic loop timer

void timerCallback(void *pArg)
{ //timer1 interrupt 1Hz

  CntLoops++;
  CntLoops = min(CntLoops, 32400);

  if (digitalRead(iPinDoor) == bDoorOpenCal)
  {
    digitalWrite(iPinLED_Door, bLightOn);
    bDoorOpen = true;
    bDoorOpenLatch = true;

    CntDoorOpen++;
    CntDoorOpen = min(CntDoorOpen, 32400);
    CntDoorOpenBeep++;
    CntDoorOpenBeep = min(CntDoorOpenBeep, 32400);

    if (CntDoorOpenBeep > CntDoorBeepThresh)
    {
      // Door has been open longer than threshold
      // cycle the audible alert
      CntDoorOpenBeep = CntDoorBeepThresh;
      bBeep = !bBeep;
      digitalWrite(iPinBeep, bBeep);
    }
  }
  else 
  {
    digitalWrite(iPinLED_Door, bLightOff);
    bBeep = false;
    bDoorOpen = false;
    CntDoorOpenBeep = 0;
    digitalWrite(iPinBeep, bBeep);
  }

  if (digitalRead(iPinMotion))
  {
    CntMotionEvents++;
    CntMotionEvents = min(CntMotionEvents, 32400);

    digitalWrite(iPinLED_Motion, bLightOn);
  }
  else
  {
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

void loop()
{

  if (CntMotionEvents > CntMotionThresh || bDoorOpenLatch || bDoorOpenLatchPrev)
  {
    CntLoopPost = CntLoopFast;
  }

  if (CntLoops >= CntLoopPost && CntLoops > 0)
  {

    Serial.println("");
    Serial.println("Calculate Data 0-2");

    Read_DHT();
    ReadLights();

    Data[0] = (float)bDoorOpen;
    Data[1] = (((float)CntDoorOpen / (float)CntLoops) * 100.0F);
    Data[2] = (((float)CntMotionEvents / (float)CntLoops) * 100.0F);
    Data[3] = (float)T_DHT;
    Data[4] = (float)PctHumidity;
    Data[5] = (float)CntLightIntensity1;
    Data[6] = (float)CntLightIntensity2;

    bDoorOpenLatchPrev = bDoorOpenLatch;
    bDoorOpenLatch = false;

    CntDoorOpen = 0;
    CntLoops = 0;
    CntLoopPost = CntLoopSlow;
    CntMotionEvents = 0;

    UpdateHomeCenter();

    PostToThingspeakFunc();

    bool bTemp;
    bTemp = WiFi.disconnect();

    Serial.println("");
    Serial.println("WiFi.disconnect = " + String((int)bTemp));
  }
}

//
//
// Function to read temperature and humidity from the DHT

void Read_DHT()
{
  int CntTempReads = 0;
  int CntHumidityReads = 0;

  T_DHT = 0.0F;
  while ((isnan(T_DHT) || T_DHT == 0.0F) &&
         (CntTempReads < 5))
  {
    // Read temperature as Fahrenheit (isFahrenheit = true)
    T_DHT = dht.readTemperature(true);
    delay(500);
    CntTempReads++;
  }

  if (isnan(T_DHT))
  {
    T_DHT = 0.0F;
  }

  Serial.println(" T_DHT       = " + String(T_DHT));

  PctHumidity = 0.0F;
  while ((isnan(PctHumidity) || PctHumidity == 0.0F) &&
         (CntHumidityReads < 5))
  {
    // Reading temperature or humidity takes about 250 ms
    PctHumidity = dht.readHumidity();
    delay(500);
    CntHumidityReads++;
  }

  if (isnan(PctHumidity))
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
  delay(200);

  CntLightIntensity1 = analogRead(A0);
  delay(200);

  digitalWrite(iPinLightD1, LOW);
  digitalWrite(iPinLightD2, HIGH);
  delay(200);

  CntLightIntensity2 = analogRead(A0);
  delay(200);

  digitalWrite(iPinLightD2, LOW);

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

  while ((WiFi.status() != WL_CONNECTED) &&
         (CntWifiRetries < CntWifiRetryAbort))
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
  String strLightOn;
  String strDoorOpen;

  const int httpPort = 80;
  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return;
  }

  String url = "";

  if (CntLightIntensity1 > CntLightOnThresh ||
      CntLightIntensity2 > CntLightOnThresh)
  {
    strLightOn = "/b" + strRoom + "LightOn=1";
  }
  else
  {
    strLightOn = "/b" + strRoom + "LightOn=0";
  }

  if (bDoorOpen)
  {
    strDoorOpen = "/b" + strRoom + "DoorOpen=1";
  }
  else
  {
    strDoorOpen = "/b" + strRoom + "DoorOpen=0";
  }

  // Send request to the home center
  client.print(String("GET ") + strLightOn + strDoorOpen +
               " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
}

//
//
// Function to post measured data to ThingSpeak

void PostToThingspeakFunc()
{

  int intThingSpeakCode;
  int CntThingSpeakRetries = 0;

  Serial.println("");
  Serial.println("Begin Post");

  ThingSpeak.begin(client);

  for (int iData = 0; iData < 8; iData++)
  {
    ThingSpeak.setField(iData + 1, Data[iData]);
  }

  intThingSpeakCode = -999;

  while (intThingSpeakCode != 200 && CntThingSpeakRetries < 5)
  {
    ThingSpeak.setField(8, (float)intThingSpeakCode);
    intThingSpeakCode = ThingSpeak.writeFields(DataChannel, DataWriteKey);
    Serial.println("WriteFields = " + String(intThingSpeakCode));

    CntThingSpeakRetries++;
    if (intThingSpeakCode != 200)
    {
      delay(15000);
    }
  }

  Serial.println("");
  Serial.println("Post to Thingspeak End");
}
