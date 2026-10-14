#ifndef HOME_ASSISTANT_WEBHOOK_H
#define HOME_ASSISTANT_WEBHOOK_H

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

class HomeAssistantWebhook {
private:
    String url;
    String lastState;

public:
    HomeAssistantWebhook(const char* webhookUrl) : url(webhookUrl), lastState("") {}

    void update(const char* state) {
        if (String(state) == lastState) return;
        
        if (WiFi.status() == WL_CONNECTED) {
            WiFiClient client;
            HTTPClient http;
            http.begin(client, url);
            http.setTimeout(1000);
            http.addHeader("Content-Type", "application/json");
            
            String payload = "{\"state\":\"" + String(state) + "\"}";
            int httpCode = http.POST(payload);
            
            if (httpCode > 0) {
                lastState = state;
                Serial.printf("HA Update (%s) sent: %d\n", url.c_str(), httpCode);
            } else {
                Serial.printf("HA Update failed (%s): %s\n", url.c_str(), http.errorToString(httpCode).c_str());
            }
            http.end();
        } else {
            Serial.println("WiFi Disconnected, skipping HA update");
        }
    }
};

#endif
