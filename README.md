# RoomMonitor

The idea for this project was inspired by my children who can never seem to fully close any door. One
particular pet peeve of mine is the sliding door of the walk out basement. Repeatedly echoing, "Make
sure
the door is closed." has translated in their heads as "slam the door as hard as possible." This usually
is
executed as push the door as hard as possible and then let go. The door then rebounds and opens back
up...
despite their "best" efforts. This arduino C++ project aims to address the door open problem by
monitoring a
sensor on the door and creating an audible tone when the door is ajar. <br>

I realized that I would also like to prevent them from leaving the lights on the basement. I added
some
light sensors to the project. To prevent false alarms during the day I needed to know if it was daylight
outside. So the project connects to a google sheet that uses a web api to get sunrise and sunset times.
Adding some functionality to the google sheet allowed me to send myself an email if the light was left
on or the door is left open.
To prevent false alarms a motion sensor was added to determine if someone is really in the basement.
<br>

At some point I thought it would be a fun idea to monitor the temperature change if the door is left
open in
winter time. I added a temperature and humidity sensor and started pushing the data to the google sheet.


## Dependecies

This arduino repo is configured to work with the following Adafruit libraries

Adafruit Unified Sensor Driver **1.1.4**
https://github.com/adafruit/Adafruit_Sensor

DHT sensor library **1.4.1**
https://github.com/adafruit/DHT-sensor-library
<br><br>

## WifiConfig.h

This project requires a file named WifiConfig.h in the same directory as the main file. This file should contain the following code with your wifi credentials.

```cpp
const char *ssid     = "WIFI_NAME";

const char *password = "WIFI_PASSWORD";
```


## SheetConfig.h

This project requires a file named SheetConfig.h in the same directory as the main file. This file should contain the following code with your google sheet id.

```cpp
const char* host = "script.google.com";

String sheet_id = "abdefghijklmnopqrstuvwxyz";
```



