// ScriptConfig.h
 
// configurations only used until first connection with google sheet
// New calibrations are pulled down each time an update is made to the sheet
const char* ha_webhook_base = "http://192.168.6.53:8123/api/webhook/";
bool bBeepEnabled = true;
 
int CntLightOnThresh = 70;
int CntWifiRetryAbort = 10;
int CntWifiFailThresh = 10;
 
int tDoorOpenAlertDelay = 300;
int tDoorOpenBeepDelay = 20;
int tLightAlertThresh = 600;
int tLightRead = 700;
int tMotionDelay = 600;
int tPost = 3600;
 
#if defined(Basement)
  const char* room_name = "Basement";
  const char* ha_temp_endpoint = "basement-temperature";
  const char* ha_motion_endpoint = "basement-motion";
  const char* ha_door_endpoint = "basement-door";
  #define USE_DHT_SENSOR
  #define USE_DOOR_SENSOR
  bool bDoorOpenDir = true;
  #define USE_LIGHT_SENSORS
  #define USE_MOTION_SENSOR
#elif defined(BonusRoom)
  const char* room_name = "BonusRoom";
  const char* ha_temp_endpoint = "bonusroom-temperature";
  const char* ha_motion_endpoint = "";
  const char* ha_door_endpoint = "";
  #define USE_AHT_SENSOR 
  bool bDoorOpenDir = false;
  // AHT and door sensors configuration conflict
#else
  #error "Please define either Basement or BonusRoom as a build flag (e.g., -D BonusRoom)"
#endif


