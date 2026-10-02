#ifndef DAYLIGHT_MANAGER_H
#define DAYLIGHT_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include <cmath>

class DaylightManager {
private:
    float latitude;
    float longitude;
    int timezone;

    const float MY_PI = 3.14159265358979323846;
    const float RAD = MY_PI / 180.0;

public:
    DaylightManager(float lat, float lon, int tz) 
        : latitude(lat), longitude(lon), timezone(tz) {}

    void setLocation(float lat, float lon, int tz) {
        latitude = lat;
        longitude = lon;
        timezone = tz;
    }

    bool isDaylight() {
        time_t now = time(nullptr);
        if (now < 100000) return true; // Time not synced yet, default to daylight

        struct tm * timeinfo = localtime(&now);
        int dayOfYear = timeinfo->tm_yday;
        
        // Simple approximation of sunrise/sunset
        // Zenith for sunrise/sunset is usually 90.833 degrees
        float zenith = 90.833 * RAD;
        float latRad = latitude * RAD;
        
        // Solar declination
        float decl = 0.409 * sin(2.0 * PI * (dayOfYear - 80) / 365.0);
        
        // Hour angle
        float cosH = (cos(zenith) - sin(latRad) * sin(decl)) / (cos(latRad) * cos(decl));
        
        if (cosH > 1.0 || cosH < -1.0) return false; // Polar night/day

        // Calculation of H is not currently used in the return logic
        // float H = acos(cosH) / RAD;

        
        // For a room monitor, a simpler approach is calculating solar noon
        // and adding/subtracting half the day length.
        float solarNoon = 720 - 4 * longitude; // minutes from midnight UTC
        float dayLength = 2 * acos(-tan(latRad) * tan(decl)) / RAD * 15.0; // degrees to minutes
        
        float sunrise = solarNoon - (dayLength / 2.0);
        float sunset = solarNoon + (dayLength / 2.0);
        
        float currentMinutes = (timeinfo->tm_hour * 60) + timeinfo->tm_min;
        // Note: timeinfo is already adjusted for timezone by configTime()
        
        // Convert UTC solar times to local time
        float localSunrise = sunrise + (timezone * 60);
        float localSunset = sunset + (timezone * 60);
        
        // Normalize to 0-1440
        auto normalize = [](float m) {
            while (m < 0) m += 1440;
            while (m >= 1440) m -= 1440;
            return m;
        };
        
        float start = normalize(localSunrise);
        float end = normalize(localSunset);
        
        if (start < end) {
            return (currentMinutes >= start && currentMinutes <= end);
        } else {
            // Sunset is next day (polar regions)
            return (currentMinutes >= start || currentMinutes <= end);
        }
    }
};

#endif
