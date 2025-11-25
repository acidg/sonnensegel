#include "mqtt_handler.h"
#include "constants.h"
#include <ArduinoJson.h>

MqttHandler* mqttHandlerInstance = nullptr;

MqttHandler::MqttHandler() 
    : mqttClient(wifiClient), lastReconnectAttempt(0), lastPublish(0), 
      connectionStartTime(0), connectingInProgress(false), failedAttempts(0), port(1883) {
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
    
    // Build Home Assistant discovery topics
    snprintf(discoveryTopic, sizeof(discoveryTopic), "homeassistant/cover/%s/config", clientId);
    snprintf(windDiscoveryTopic, sizeof(windDiscoveryTopic), "homeassistant/sensor/%s_wind/config", clientId);
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
    mqttClient.setBufferSize(1536);
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
    mqttClient.subscribe(setWindThresholdTopic);
}

void MqttHandler::publishDiscovery() {
    if (!isConnected()) {
        return;
    }
    
    // Publish cover discovery message
    {
        DynamicJsonDocument doc(1024);
        
        doc["name"] = "Awning";
        doc["unique_id"] = clientId;
        doc["command_topic"] = commandTopic;
        doc["state_topic"] = stateTopic;
        doc["position_topic"] = positionTopic;
        doc["set_position_topic"] = setPositionTopic;
        doc["availability_topic"] = availabilityTopic;
        
        // Command payloads
        doc["payload_open"] = "OPEN";
        doc["payload_close"] = "CLOSE";
        doc["payload_stop"] = "STOP";
        
        // State payloads
        doc["state_open"] = "open";
        doc["state_opening"] = "opening";
        doc["state_closed"] = "closed";
        doc["state_closing"] = "closing";
        doc["state_stopped"] = "stopped";
        
        // Position configuration
        doc["position_open"] = 100;
        doc["position_closed"] = 0;
        
        // Device information
        JsonObject device = doc.createNestedObject("device");
        device["identifiers"][0] = clientId;
        device["name"] = "Awning Controller";
        device["manufacturer"] = "DIY";
        device["model"] = "ESP8266 Awning Controller";
        device["sw_version"] = "1.0";
        
        char buffer[1024];
        serializeJson(doc, buffer);
        mqttClient.publish(discoveryTopic, buffer, true);
        
        Serial.print("Published discovery to: ");
        Serial.println(discoveryTopic);
    }
    
    // Publish wind sensor discovery message
    {
        DynamicJsonDocument doc(512);
        
        doc["name"] = "Awning Wind Sensor";
        doc["unique_id"] = String(clientId) + "_wind";
        doc["state_topic"] = windPulsesTopic;
        doc["availability_topic"] = availabilityTopic;
        doc["unit_of_measurement"] = "pulses/min";
        doc["icon"] = "mdi:weather-windy";
        
        // Device information (same device)
        JsonObject device = doc.createNestedObject("device");
        device["identifiers"][0] = clientId;
        device["name"] = "Awning Controller";
        device["manufacturer"] = "DIY";
        device["model"] = "ESP8266 Awning Controller";
        device["sw_version"] = "1.0";
        
        char buffer[512];
        serializeJson(doc, buffer);
        mqttClient.publish(windDiscoveryTopic, buffer, true);
        
        Serial.print("Published wind sensor discovery to: ");
        Serial.println(windDiscoveryTopic);
    }
}

bool MqttHandler::reconnect() {
    if (!WiFi.isConnected()) {
        connectingInProgress = false;
        return false;
    }
    
    unsigned long now = millis();
    
    // Check if we should start a new connection attempt
    if (!connectingInProgress) {
        // Calculate backoff delay based on failed attempts
        unsigned long backoffDelay = MQTT_RECONNECT_INTERVAL_MS;  // Start with 5 seconds
        if (failedAttempts > 0) {
            backoffDelay = MQTT_BACKOFF_BASE_MS * min(failedAttempts, 4UL);  // Then 30s, 60s, 90s, 120s
        }
        
        if (now - lastReconnectAttempt < backoffDelay) {
            return false;
        }
        
        // Start new connection attempt
        connectingInProgress = true;
        connectionStartTime = now;
        lastReconnectAttempt = now;
        
        Serial.print("Attempting MQTT connection (attempt ");
        Serial.print(failedAttempts + 1);
        Serial.println(")");
        Serial.print("MQTT Config - Server: ");
        Serial.print(server);
        Serial.print(", Port: ");
        Serial.print(port);
        Serial.print(", ClientID: ");
        Serial.print(clientId);
        Serial.print(", Username: ");
        Serial.print(strlen(username) > 0 ? username : "(none)");
        Serial.print(", HasPassword: ");
        Serial.print(strlen(password) > 0 ? "yes" : "no");
        Serial.print(", AvailabilityTopic: ");
        Serial.println(availabilityTopic);
        
        // Set very short timeout to prevent blocking
        wifiClient.setTimeout(1000);
        mqttClient.setSocketTimeout(1);
        
        bool connected;
        if (strlen(username) > 0) {
            connected = mqttClient.connect(clientId, username, password, 
                                         availabilityTopic, 0, true, "offline");
        } else {
            connected = mqttClient.connect(clientId, availabilityTopic, 0, true, "offline");
        }
        
        connectingInProgress = false;
        
        if (!connected) {
            failedAttempts++;
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.print(" (attempt ");
            Serial.print(failedAttempts);
            Serial.println(")");
            
            if (failedAttempts >= MQTT_MAX_FAILED_ATTEMPTS) {
                Serial.println("MQTT: Max connection attempts reached, backing off");
            }
            return false;
        }
        
        // Connection successful
        failedAttempts = 0;
        Serial.println("connected");
        mqttClient.publish(availabilityTopic, "online", true);
        subscribe();
        publishDiscovery();
        return true;
    }
    
    // Check for connection timeout
    if (now - connectionStartTime > MQTT_CONNECTION_TIMEOUT_MS) {
        Serial.println("MQTT connection timeout");
        connectingInProgress = false;
        failedAttempts++;
        wifiClient.stop();
        return false;
    }
    
    return false;
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
    
    // Determine state based on motor state and position
    const char* state;
    if (motorState == MOTOR_EXTENDING) {
        state = "opening";
    } else if (motorState == MOTOR_RETRACTING) {
        state = "closing";
    } else {
        // Motor is stopped - determine if open, closed, or stopped
        if (position >= 99.0) {
            state = "open";
        } else if (position <= 1.0) {
            state = "closed";
        } else {
            state = "stopped";
        }
    }
    
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
    } else if (strcmp(topic, setWindThresholdTopic) == 0 && onSetWindThreshold) {
        float threshold = atof(message);
        onSetWindThreshold(threshold);
    }
}