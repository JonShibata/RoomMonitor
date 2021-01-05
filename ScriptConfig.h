// ScriptConfig.h


#define Basement
// #define Garage
// #define Test


#ifdef Basement
const char* room_name = "Basement";
bool        eDoorOpenCal = true;
#endif

#ifdef Garage
const char* room_name = "Garage";
bool        eDoorOpenCal = false;
#endif

#ifdef Test
const char* room_name = "Test";
bool        eDoorOpenCal = true;
#endif



int CntDoorOpenBeepDelay = 30;
int CntLightOnThresh     = 100;
int CntMotionDelay       = 300;
int CntWifiRetryAbort    = 10;
int CntWifiFailThresh    = 10;
int CntLoopPost          = 30;


String HomeAlertIP = "192.168.86.200";  // ip address of the home center

