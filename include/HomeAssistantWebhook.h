#ifndef HOME_ASSISTANT_WEBHOOK_H
#define HOME_ASSISTANT_WEBHOOK_H

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

class HomeAssistantWebhook {
private:
    String url;
    String lastState;

public:
    HomeAssistantWebhook(const String& webhookUrl) : url(webhookUrl), lastState("") {}

    void update(const char* state, bool force = false, const char* key = "state") {
        if (url == "" || url.endsWith("/webhook/")) return;
        if (!force && String(state) == lastState) return;
        
        if (WiFi.status() == WL_CONNECTED) {
            int retries = 3;
            int attempt = 0;
            bool success = false;

            while (attempt < retries && !success) {
                WiFiClient client;
                HTTPClient http;
                http.begin(client, url);
                http.setTimeout(1000);
                http.addHeader("Content-Type", "application/json");
                
                String payload = "{\"" + String(key) + "\":\"" + String(state) + "\"}";
                int httpCode = http.POST(payload);
                
                if (httpCode == HTTP_CODE_OK) {
                    success = true;
                    lastState = state;
                    Serial.printf("HA Update (%s) sent: %d\n", url.c_str(), httpCode);
                } else {
                    attempt++;
                    Serial.printf("HA Update failed (%s): %s, attempt %d/%d\n", 
                                  url.c_str(), http.errorToString(httpCode).c_str(), attempt, retries);
                    if (attempt < retries) delay(100);
                }
                http.end();
            }
        } else {
            Serial.println("WiFi Disconnected, skipping HA update");
        }
    }

    void forceSend(const char* state) {
        update(state, true);
    }
};

#endif
