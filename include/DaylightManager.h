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

        // For a room monitor, a simpler approach is calculating solar noon
        // and adding/subtracting half the day length.
        float solarNoon = 720 - (longitude / 15.0) * 60.0; // minutes from midnight UTC
        
        // Correct Day Length: H = acos(-tan(lat)*tan(decl)). DayLength = 2 * H * (180/PI) / 15 * 60 mins
        float H = acos(-tan(latRad) * tan(decl));
        float dayLengthMins = 2.0 * (H / RAD) * 4.0; // 1 degree = 4 minutes
        
        float sunrise = solarNoon - (dayLengthMins / 2.0);
        float sunset = solarNoon + (dayLengthMins / 2.0);
        
        // Convert current local time to UTC minutes from midnight
        float localMinutes = (timeinfo->tm_hour * 60) + timeinfo->tm_min;
        float utcMinutes = localMinutes - (timezone * 60);
        
        // Normalize UTC minutes to 0-1440
        auto normalize = [](float m) {
            while (m < 0) m += 1440;
            while (m >= 1440) m -= 1440;
            return m;
        };
        
        float currentUTC = normalize(utcMinutes);
        float start = normalize(sunrise);
        float end = normalize(sunset);
        
        if (start < end) {
            return (currentUTC >= start && currentUTC <= end);
        } else {
            // Sunset is next day (polar regions)
            return (currentUTC >= start || currentUTC <= end);
        }
    }
};

#endif

