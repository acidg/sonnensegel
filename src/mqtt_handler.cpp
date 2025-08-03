#include "mqtt_handler.h"

MqttHandler* mqttHandlerInstance = nullptr;

MqttHandler::MqttHandler() 
    : mqttClient(wifiClient), lastReconnectAttempt(0), lastPublish(0) {
    mqttHandlerInstance = this;
}

void MqttHandler::buildTopics() {
    snprintf(stateTopic, sizeof(stateTopic), "%s/state", MQTT_BASE_TOPIC);
    snprintf(commandTopic, sizeof(commandTopic), "%s/set", MQTT_BASE_TOPIC);
    snprintf(positionTopic, sizeof(positionTopic), "%s/position", MQTT_BASE_TOPIC);
    snprintf(setPositionTopic, sizeof(setPositionTopic), "%s/set_position", MQTT_BASE_TOPIC);
    snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/availability", MQTT_BASE_TOPIC);
    snprintf(windPulsesTopic, sizeof(windPulsesTopic), "%s/wind_pulses", MQTT_BASE_TOPIC);
    snprintf(windThresholdTopic, sizeof(windThresholdTopic), "%s/wind_threshold", MQTT_BASE_TOPIC);
    snprintf(setWindThresholdTopic, sizeof(setWindThresholdTopic), "%s/set_wind_threshold", MQTT_BASE_TOPIC);
    snprintf(calibrateTopic, sizeof(calibrateTopic), "%s/calibrate", MQTT_BASE_TOPIC);
}

void MqttHandler::begin() {
    buildTopics();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(staticCallback);
    mqttClient.setBufferSize(512);
}

void MqttHandler::staticCallback(char* topic, byte* payload, unsigned int length) {
    if (mqttHandlerInstance) {
        char message[length + 1];
        memcpy(message, payload, length);
        message[length] = '\0';
        mqttHandlerInstance->processMessage(topic, message);
    }
}

void MqttHandler::subscribe() {
    mqttClient.subscribe(commandTopic);
    mqttClient.subscribe(setPositionTopic);
    mqttClient.subscribe(calibrateTopic);
    mqttClient.subscribe(setWindThresholdTopic);
}

bool MqttHandler::reconnect() {
    if (!WiFi.isConnected()) {
        return false;
    }
    
    unsigned long now = millis();
    if (now - lastReconnectAttempt < MQTT_RECONNECT_INTERVAL_MS) {
        return false;
    }
    
    lastReconnectAttempt = now;
    
    Serial.print("Attempting MQTT connection...");
    
    if (!mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, 
                          availabilityTopic, 0, true, "offline")) {
        Serial.print("failed, rc=");
        Serial.println(mqttClient.state());
        return false;
    }
    
    Serial.println("connected");
    mqttClient.publish(availabilityTopic, "online", true);
    subscribe();
    return true;
}

void MqttHandler::loop() {
    if (!WiFi.isConnected()) {
        return;
    }
    
    if (!mqttClient.connected()) {
        reconnect();
        return;
    }
    
    mqttClient.loop();
}

void MqttHandler::publishState(MotorState motorState, float position) {
    if (!isConnected()) {
        return;
    }
    
    unsigned long now = millis();
    if (now - lastPublish < MQTT_PUBLISH_INTERVAL_MS) {
        return;
    }
    
    lastPublish = now;
    
    const char* state = "stopped";
    if (motorState == MOTOR_EXTENDING) state = "opening";
    else if (motorState == MOTOR_RETRACTING) state = "closing";
    
    mqttClient.publish(stateTopic, state, true);
    
    char positionStr[10];
    dtostrf(position, 4, 1, positionStr);
    mqttClient.publish(positionTopic, positionStr, true);
}

void MqttHandler::publishWindData(unsigned long pulses, unsigned long threshold) {
    if (!isConnected()) {
        return;
    }
    
    char pulsesStr[10];
    sprintf(pulsesStr, "%lu", pulses);
    mqttClient.publish(windPulsesTopic, pulsesStr, true);
    
    char thresholdStr[10];
    sprintf(thresholdStr, "%lu", threshold);
    mqttClient.publish(windThresholdTopic, thresholdStr, true);
}

void MqttHandler::processMessage(char* topic, char* message) {
    Serial.print("MQTT message [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
    
    if (strcmp(topic, commandTopic) == 0 && onCommand) {
        onCommand(message);
    } else if (strcmp(topic, setPositionTopic) == 0 && onSetPosition) {
        float position = atof(message);
        onSetPosition(position);
    } else if (strcmp(topic, calibrateTopic) == 0 && onCalibrate) {
        unsigned long travelTime = atol(message);
        onCalibrate(travelTime);
    } else if (strcmp(topic, setWindThresholdTopic) == 0 && onSetWindThreshold) {
        float threshold = atof(message);
        onSetWindThreshold(threshold);
    }
}