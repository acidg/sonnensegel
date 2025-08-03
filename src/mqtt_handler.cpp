#include "mqtt_handler.h"

MqttHandler* mqttHandlerInstance = nullptr;

MqttHandler::MqttHandler() 
    : mqttClient(wifiClient), lastReconnectAttempt(0), lastPublish(0), port(1883) {
    mqttHandlerInstance = this;
    strcpy(server, "");
    strcpy(username, "");
    strcpy(password, "");
    strcpy(clientId, "awning_controller");
    strcpy(baseTopic, "home/awning");
}

void MqttHandler::buildTopics() {
    snprintf(stateTopic, sizeof(stateTopic), "%s/state", baseTopic);
    snprintf(commandTopic, sizeof(commandTopic), "%s/set", baseTopic);
    snprintf(positionTopic, sizeof(positionTopic), "%s/position", baseTopic);
    snprintf(setPositionTopic, sizeof(setPositionTopic), "%s/set_position", baseTopic);
    snprintf(availabilityTopic, sizeof(availabilityTopic), "%s/availability", baseTopic);
    snprintf(windPulsesTopic, sizeof(windPulsesTopic), "%s/wind_pulses", baseTopic);
    snprintf(windThresholdTopic, sizeof(windThresholdTopic), "%s/wind_threshold", baseTopic);
    snprintf(setWindThresholdTopic, sizeof(setWindThresholdTopic), "%s/set_wind_threshold", baseTopic);
    snprintf(calibrateTopic, sizeof(calibrateTopic), "%s/calibrate", baseTopic);
}

void MqttHandler::begin(const char* srv, uint16_t prt, const char* user, 
                       const char* pass, const char* cid) {
    strncpy(server, srv, sizeof(server) - 1);
    server[sizeof(server) - 1] = '\0';
    port = prt;
    strncpy(username, user, sizeof(username) - 1);
    username[sizeof(username) - 1] = '\0';
    strncpy(password, pass, sizeof(password) - 1);
    password[sizeof(password) - 1] = '\0';
    strncpy(clientId, cid, sizeof(clientId) - 1);
    clientId[sizeof(clientId) - 1] = '\0';
    
    buildTopics();
    mqttClient.setServer(server, port);
    mqttClient.setCallback(staticCallback);
    mqttClient.setBufferSize(512);
}

void MqttHandler::setBaseTopic(const char* topic) {
    strncpy(baseTopic, topic, sizeof(baseTopic) - 1);
    baseTopic[sizeof(baseTopic) - 1] = '\0';
    buildTopics();
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
    
    bool connected;
    if (strlen(username) > 0) {
        connected = mqttClient.connect(clientId, username, password, 
                                     availabilityTopic, 0, true, "offline");
    } else {
        connected = mqttClient.connect(clientId, availabilityTopic, 0, true, "offline");
    }
    
    if (!connected) {
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