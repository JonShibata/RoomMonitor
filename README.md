# RoomMonitor

## Dependencies

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

## OTA Updates (PlatformIO)
 
This project supports Over-The-Air (OTA) updates. To update the firmware wirelessly:
 
1. Ensure the device is powered on and connected to the same network as your PC.
2. In PlatformIO, run the target `UploadOTA` instead of the standard `Upload`.
3. To update the web interface files, run the `Upload Filesystem Image` task to flash the `/data` folder to LittleFS.
 
## Configuration and Security
 
The device uses LittleFS to store runtime settings in `config.json`. 
 
**Important:** Settings saved via the web `/save` endpoint (such as MQTT credentials) are stored in the filesystem and will **persist across firmware reflashes**. 
 
To update MQTT credentials securely:
- Use the web interface to send a POST request to `/save` with `mqtt_user` and `mqtt_pass`.
- This prevents sensitive credentials from being hardcoded in the source code.

