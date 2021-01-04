// ScriptConfig.h


int  CntDoorOpenBeepDelay = 30;
int  CntLightOnThresh     = 100;
int  CntMotionDelay       = 300;
int  CntWifiRetryAbort    = 10;
int  CntWifiFailThresh    = 10;
int  CntLoopPost          = 1800;
bool eDoorOpenCal         = false;  // Basement=true, Garage=?

// String host = "192.168.86.200";  // ip address of the home center

const char* esp_hostname = "Basement";  // Basement, Garage, Test
const char* room_name   = esp_hostname;





// example
// https://script.google.com/macros/s/AKfycbyCEoao5z9EZcphU0m_rxS1WVS5ibpxrqObGfinC8qb7ewiO12l/exec?room_name=Test&Door=10&Temperature=9&Humidity=8&Motion=7&Light1=6&Light2=7
