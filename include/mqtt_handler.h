#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "motor_controller.h"
#include "position_tracker.h"
#include "wind_sensor.h"

class MqttHandler {
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt;
    unsigned long lastPublish;
    
    // Topic buffers
    char stateTopic[128];
    char commandTopic[128];
    char positionTopic[128];
    char setPositionTopic[128];
    char availabilityTopic[128];
    char windSpeedTopic[128];
    char windThresholdTopic[128];
    char setWindThresholdTopic[128];
    char calibrateTopic[128];
    char windFactorTopic[128];
    char setWindFactorTopic[128];
    
    void buildTopics();
    bool reconnect();
    void subscribe();
    static void staticCallback(char* topic, byte* payload, unsigned int length);
    
public:
    MqttHandler();
    void begin();
    void setCallback(std::function<void(char*, byte*, unsigned int)> callback);
    void loop();
    void publishState(MotorState motorState, float position);
    void publishWindData(float speed, float threshold, float factor);
    bool isConnected() const { return mqttClient.connected(); }
    void processMessage(char* topic, char* message);
    
    // Callbacks for commands
    std::function<void(const char*)> onCommand;
    std::function<void(float)> onSetPosition;
    std::function<void(unsigned long)> onCalibrate;
    std::function<void(float)> onSetWindThreshold;
    std::function<void(float)> onSetWindFactor;
};

// Global instance for static callback
extern MqttHandler* mqttHandlerInstance;

#endif // MQTT_HANDLER_H