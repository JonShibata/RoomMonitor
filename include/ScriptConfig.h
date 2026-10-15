// ScriptConfig.h
 
// configurations only used until first connection with google sheet
// New calibrations are pulled down each time an update is made to the sheet
const char* ha_webhook_base = "http://192.168.6.53:8123/api/webhook/";

#if defined(basement)
  const char* room_name = "basement";
  const char* ha_temp_endpoint = "basement-temperature";
  const char* ha_motion_endpoint = "basement-motion";
  const char* ha_door_endpoint = "basement-door";
  #define USE_DHT_SENSOR
  #define USE_DOOR_SENSOR
  #define USE_LIGHT_SENSORS
  #define USE_MOTION_SENSOR
#elif defined(bonusroom)
  const char* room_name = "bonusroom";
  const char* ha_temp_endpoint = "bonusroom-temperature";
  const char* ha_motion_endpoint = "";
  const char* ha_door_endpoint = "";
  #define USE_AHT_SENSOR 
  // AHT and door sensors configuration conflict
#else
  #error "Please define either basement or bonusroom as a build flag (e.g., -D bonusroom)"
#endif


