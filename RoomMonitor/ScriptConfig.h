// ScriptConfig.h


 #define Basement
// #define Garage
//#define Test



#ifdef Basement
const char* esp_hostname = "Basement"; 
bool eDoorOpenCal         = true;  
#endif

#ifdef Garage
const char* esp_hostname = "Garage"; 
bool eDoorOpenCal         = false; 
#endif

#ifdef Test
const char* esp_hostname = "Test";  
bool eDoorOpenCal         = true;  
#endif


const char* room_name   = esp_hostname;


int  CntDoorOpenBeepDelay = 30;
int  CntLightOnThresh     = 100;
int  CntMotionDelay       = 300;
int  CntWifiRetryAbort    = 10;
int  CntWifiFailThresh    = 10;
int  CntLoopPost          = 1800;


// String host = "192.168.86.200";  // ip address of the home center








// example
// https://script.google.com/macros/s/AKfycbyCEoao5z9EZcphU0m_rxS1WVS5ibpxrqObGfinC8qb7ewiO12l/exec?room_name=Test&Door=10&Temperature=9&Humidity=8&Motion=7&Light1=6&Light2=7
