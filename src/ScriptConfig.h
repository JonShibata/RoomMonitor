// ScriptConfig.h

// configurations only used until first connection with google sheet
// New calibrations are pulled down each time an update is made to the sheet
bool bBeepEnabled = true;

int CntLightOnThresh = 70;
int CntWifiRetryAbort = 10;
int CntWifiFailThresh = 10;

int tDoorOpenAlertDelay = 300;
int tDoorOpenBeepDelay = 20;
int tLightAlertThresh = 600;
int tLightRead = 700;
int tMotionDelay = 600;
int tPost = 30;

String HomeAlertIP = "192.168.68.200";  // ip address of the home center

#define Basement
// #define BonusRoom
// #define Garage
// #define Test

#ifdef Basement
  const char* room_name = "Basement";
  #define USE_DHT_SENSOR
  #define USE_DOOR_SENSOR
  bool bDoorOpenDir = true;
  #define USE_LIGHT_SENSORS
  #define USE_MOTION_SENSOR
#endif

#ifdef BonusRoom
  const char* room_name = "BonusRoom";
  #define USE_AHT_SENSOR 
  bool bDoorOpenDir = false;
  // AHT and door sensors configuration conflict
#endif

#ifdef Garage
  const char* room_name = "Garage";
#endif

#ifdef Test
  const char* room_name = "Test";
#endif

