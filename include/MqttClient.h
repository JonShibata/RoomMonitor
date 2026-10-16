#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H
 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "ConfigManager.h"
#include "ScriptConfig.h"
 
class MqttClient {
private:
    WiFiClient espClient;
    PubSubClient client;
    String room;
 
public:
    MqttClient() : client(espClient), room(room_name) {}
 
    void setup(ConfigManager& configMgr) {
        client.setServer(configMgr.settings.mqtt_broker, 1883);
    }
 
    void loop(ConfigManager& configMgr) {
        if (!client.connected()) {
            reconnect(configMgr);
        }
        client.loop();
    }
 
    void reconnect(ConfigManager& configMgr) {
        if (WiFi.status() != WL_CONNECTED) return;
 
        Serial.print("Attempting MQTT connection...");
        if (client.connect(room.c_str(), configMgr.settings.mqtt_user.c_str(), configMgr.settings.mqtt_pass.c_str())) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
 
    bool isConnected() {
        return client.connected();
    }
 
    void publish(const String& sensor, const char* value) {
        if (!client.connected()) return;
        
        String topic = room + "/" + sensor;
        if (client.publish(topic.c_str(), value)) {
            Serial.printf("MQTT Published: %s -> %s\n", topic.c_str(), value);
        } else {
            Serial.printf("MQTT Publish failed: %s\n", topic.c_str());
        }
    }
};
 
#endif

