#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "config.h"

// GPIO Pin Definitions
const uint8_t RELAY_EXTEND = 14;
const uint8_t RELAY_RETRACT = 12;
const uint8_t BUTTON_EXTEND = 2;
const uint8_t BUTTON_RETRACT = 4;
const uint8_t WIND_SENSOR_PIN = 5;

// Timing Constants
const unsigned long MOTOR_START_PULSE_MS = 1000;
const unsigned long MOTOR_STOP_PULSE_MS = 100;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long BUTTON_LONG_PRESS_MS = 1000;
const unsigned long POSITION_UPDATE_INTERVAL_MS = 100;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long WIND_MEASUREMENT_INTERVAL_MS = 1000;
const unsigned long WIND_SAFETY_CHECK_INTERVAL_MS = 5000;

// EEPROM Addresses
const int EEPROM_SIZE = 512;
const int EEPROM_POSITION_ADDR = 0;
const int EEPROM_TRAVEL_TIME_ADDR = 4;
const int EEPROM_WIND_THRESHOLD_ADDR = 8;
const int EEPROM_WIND_FACTOR_ADDR = 12;
const int EEPROM_MAGIC_ADDR = 16;
const uint32_t EEPROM_MAGIC_VALUE = 0xDEADBEEF;

// Motor States
enum MotorState {
    MOTOR_IDLE,
    MOTOR_EXTENDING,
    MOTOR_RETRACTING,
    MOTOR_STOPPING
};

// Button States
struct ButtonState {
    bool lastState;
    bool currentState;
    unsigned long lastDebounceTime;
    unsigned long pressStartTime;
    bool longPressHandled;
};

// Global Variables
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
MotorState motorState = MOTOR_IDLE;
float currentPosition = 0.0;
float targetPosition = 0.0;
unsigned long travelTimeMs = DEFAULT_TRAVEL_TIME_MS;
unsigned long motorStartTime = 0;
unsigned long lastPositionUpdate = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastWindMeasurement = 0;
unsigned long lastWindSafetyCheck = 0;
ButtonState extendButton = {HIGH, HIGH, 0, 0, false};
ButtonState retractButton = {HIGH, HIGH, 0, 0, false};

// Wind Sensor Variables
volatile unsigned long windPulseCount = 0;
unsigned long lastWindPulseCount = 0;
float currentWindSpeed = 0.0;
float windSpeedThreshold = DEFAULT_WIND_THRESHOLD;
float windConversionFactor = DEFAULT_WIND_FACTOR;
bool windSafetyTriggered = false;

// MQTT Topics
char mqttStateTopic[128];
char mqttCommandTopic[128];
char mqttPositionTopic[128];
char mqttSetPositionTopic[128];
char mqttAvailabilityTopic[128];
char mqttWindSpeedTopic[128];
char mqttWindThresholdTopic[128];
char mqttSetWindThresholdTopic[128];
char mqttCalibrateTopic[128];
char mqttWindFactorTopic[128];
char mqttSetWindFactorTopic[128];

void ICACHE_RAM_ATTR windSensorISR() {
    windPulseCount++;
}

void setupPins() {
    pinMode(RELAY_EXTEND, OUTPUT);
    pinMode(RELAY_RETRACT, OUTPUT);
    pinMode(BUTTON_EXTEND, INPUT_PULLUP);
    pinMode(BUTTON_RETRACT, INPUT_PULLUP);
    pinMode(WIND_SENSOR_PIN, INPUT_PULLUP);
    
    digitalWrite(RELAY_EXTEND, LOW);
    digitalWrite(RELAY_RETRACT, LOW);
    
    attachInterrupt(digitalPinToInterrupt(WIND_SENSOR_PIN), windSensorISR, FALLING);
}

void loadFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    
    uint32_t magic;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    
    if (magic == EEPROM_MAGIC_VALUE) {
        EEPROM.get(EEPROM_POSITION_ADDR, currentPosition);
        EEPROM.get(EEPROM_TRAVEL_TIME_ADDR, travelTimeMs);
        EEPROM.get(EEPROM_WIND_THRESHOLD_ADDR, windSpeedThreshold);
        EEPROM.get(EEPROM_WIND_FACTOR_ADDR, windConversionFactor);
        
        currentPosition = constrain(currentPosition, 0.0, 100.0);
        travelTimeMs = constrain(travelTimeMs, 5000UL, 300000UL);
        windSpeedThreshold = constrain(windSpeedThreshold, 0.0, 100.0);
        windConversionFactor = constrain(windConversionFactor, 0.1, 10.0);
    } else {
        currentPosition = 0.0;
        travelTimeMs = DEFAULT_TRAVEL_TIME_MS;
        windSpeedThreshold = DEFAULT_WIND_THRESHOLD;
        windConversionFactor = DEFAULT_WIND_FACTOR;
        saveToEEPROM();
    }
    
    targetPosition = currentPosition;
}

void saveToEEPROM() {
    EEPROM.put(EEPROM_POSITION_ADDR, currentPosition);
    EEPROM.put(EEPROM_TRAVEL_TIME_ADDR, travelTimeMs);
    EEPROM.put(EEPROM_WIND_THRESHOLD_ADDR, windSpeedThreshold);
    EEPROM.put(EEPROM_WIND_FACTOR_ADDR, windConversionFactor);
    EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
    EEPROM.commit();
}

void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed - continuing without network");
    }
}

void buildMqttTopics() {
    snprintf(mqttStateTopic, sizeof(mqttStateTopic), "%s/state", MQTT_BASE_TOPIC);
    snprintf(mqttCommandTopic, sizeof(mqttCommandTopic), "%s/set", MQTT_BASE_TOPIC);
    snprintf(mqttPositionTopic, sizeof(mqttPositionTopic), "%s/position", MQTT_BASE_TOPIC);
    snprintf(mqttSetPositionTopic, sizeof(mqttSetPositionTopic), "%s/set_position", MQTT_BASE_TOPIC);
    snprintf(mqttAvailabilityTopic, sizeof(mqttAvailabilityTopic), "%s/availability", MQTT_BASE_TOPIC);
    snprintf(mqttWindSpeedTopic, sizeof(mqttWindSpeedTopic), "%s/wind_speed", MQTT_BASE_TOPIC);
    snprintf(mqttWindThresholdTopic, sizeof(mqttWindThresholdTopic), "%s/wind_threshold", MQTT_BASE_TOPIC);
    snprintf(mqttSetWindThresholdTopic, sizeof(mqttSetWindThresholdTopic), "%s/set_wind_threshold", MQTT_BASE_TOPIC);
    snprintf(mqttCalibrateTopic, sizeof(mqttCalibrateTopic), "%s/calibrate", MQTT_BASE_TOPIC);
    snprintf(mqttWindFactorTopic, sizeof(mqttWindFactorTopic), "%s/wind_factor", MQTT_BASE_TOPIC);
    snprintf(mqttSetWindFactorTopic, sizeof(mqttSetWindFactorTopic), "%s/set_wind_factor", MQTT_BASE_TOPIC);
}

void publishState() {
    if (!mqttClient.connected()) return;
    
    const char* state = "stopped";
    if (motorState == MOTOR_EXTENDING) state = "opening";
    else if (motorState == MOTOR_RETRACTING) state = "closing";
    
    mqttClient.publish(mqttStateTopic, state, true);
    
    char positionStr[10];
    dtostrf(currentPosition, 4, 1, positionStr);
    mqttClient.publish(mqttPositionTopic, positionStr, true);
    
    char windSpeedStr[10];
    dtostrf(currentWindSpeed, 4, 1, windSpeedStr);
    mqttClient.publish(mqttWindSpeedTopic, windSpeedStr, true);
    
    char windThresholdStr[10];
    dtostrf(windSpeedThreshold, 4, 1, windThresholdStr);
    mqttClient.publish(mqttWindThresholdTopic, windThresholdStr, true);
    
    char windFactorStr[10];
    dtostrf(windConversionFactor, 4, 2, windFactorStr);
    mqttClient.publish(mqttWindFactorTopic, windFactorStr, true);
}

void sendMotorPulse(uint8_t relayPin, unsigned long duration) {
    if (digitalRead(RELAY_EXTEND) == HIGH || digitalRead(RELAY_RETRACT) == HIGH) {
        return;
    }
    
    digitalWrite(relayPin, HIGH);
    delay(duration);
    digitalWrite(relayPin, LOW);
}

void startMotor(MotorState direction) {
    if (motorState != MOTOR_IDLE) {
        stopMotor();
        delay(500);
    }
    
    if (direction == MOTOR_EXTENDING) {
        if (currentPosition >= 100.0) return;
        sendMotorPulse(RELAY_EXTEND, MOTOR_START_PULSE_MS);
        motorState = MOTOR_EXTENDING;
    } else if (direction == MOTOR_RETRACTING) {
        if (currentPosition <= 0.0) return;
        sendMotorPulse(RELAY_RETRACT, MOTOR_START_PULSE_MS);
        motorState = MOTOR_RETRACTING;
    }
    
    motorStartTime = millis();
    publishState();
}

void stopMotor() {
    if (motorState == MOTOR_IDLE) return;
    
    motorState = MOTOR_STOPPING;
    
    if (digitalRead(RELAY_EXTEND) == HIGH || digitalRead(RELAY_RETRACT) == HIGH) {
        digitalWrite(RELAY_EXTEND, LOW);
        digitalWrite(RELAY_RETRACT, LOW);
        delay(100);
    }
    
    sendMotorPulse(RELAY_EXTEND, MOTOR_STOP_PULSE_MS);
    
    motorState = MOTOR_IDLE;
    targetPosition = currentPosition;
    saveToEEPROM();
    publishState();
}

void updatePosition() {
    if (motorState == MOTOR_IDLE) return;
    
    unsigned long now = millis();
    if (now - lastPositionUpdate < POSITION_UPDATE_INTERVAL_MS) return;
    
    unsigned long elapsedTime = now - motorStartTime;
    float percentageChange = (float)elapsedTime / (float)travelTimeMs * 100.0;
    
    if (motorState == MOTOR_EXTENDING) {
        currentPosition = constrain(currentPosition + percentageChange * (now - lastPositionUpdate) / travelTimeMs, 0.0, 100.0);
        if (currentPosition >= targetPosition || currentPosition >= 100.0) {
            stopMotor();
        }
    } else if (motorState == MOTOR_RETRACTING) {
        currentPosition = constrain(currentPosition - percentageChange * (now - lastPositionUpdate) / travelTimeMs, 0.0, 100.0);
        if (currentPosition <= targetPosition || currentPosition <= 0.0) {
            stopMotor();
        }
    }
    
    lastPositionUpdate = now;
    
    static unsigned long lastPublish = 0;
    if (now - lastPublish > 1000) {
        publishState();
        lastPublish = now;
    }
}

void updateWindSpeed() {
    unsigned long now = millis();
    if (now - lastWindMeasurement < WIND_MEASUREMENT_INTERVAL_MS) return;
    
    unsigned long pulseDelta = windPulseCount - lastWindPulseCount;
    float timeDelta = (now - lastWindMeasurement) / 1000.0;
    
    float pulsesPerSecond = pulseDelta / timeDelta;
    currentWindSpeed = pulsesPerSecond * windConversionFactor;
    
    lastWindPulseCount = windPulseCount;
    lastWindMeasurement = now;
    
    static unsigned long lastPublish = 0;
    if (now - lastPublish > 5000) {
        char windSpeedStr[10];
        dtostrf(currentWindSpeed, 4, 1, windSpeedStr);
        mqttClient.publish(mqttWindSpeedTopic, windSpeedStr, true);
        lastPublish = now;
    }
}

void checkWindSafety() {
    unsigned long now = millis();
    if (now - lastWindSafetyCheck < WIND_SAFETY_CHECK_INTERVAL_MS) return;
    lastWindSafetyCheck = now;
    
    if (windSpeedThreshold > 0 && currentWindSpeed > windSpeedThreshold) {
        if (!windSafetyTriggered) {
            windSafetyTriggered = true;
            Serial.print("Wind safety triggered! Speed: ");
            Serial.print(currentWindSpeed);
            Serial.print(" > Threshold: ");
            Serial.println(windSpeedThreshold);
            
            if (currentPosition > 0.0) {
                targetPosition = 0.0;
                startMotor(MOTOR_RETRACTING);
            }
        }
    } else {
        windSafetyTriggered = false;
    }
}

void moveToPosition(float position) {
    position = constrain(position, 0.0, 100.0);
    targetPosition = position;
    
    if (abs(currentPosition - targetPosition) < 1.0) {
        return;
    }
    
    if (targetPosition > currentPosition) {
        startMotor(MOTOR_EXTENDING);
    } else {
        startMotor(MOTOR_RETRACTING);
    }
}

void handleButton(ButtonState& button, uint8_t pin, MotorState motorDirection) {
    bool reading = digitalRead(pin);
    
    if (reading != button.lastState) {
        button.lastDebounceTime = millis();
    }
    
    if ((millis() - button.lastDebounceTime) > BUTTON_DEBOUNCE_MS) {
        if (reading != button.currentState) {
            button.currentState = reading;
            
            if (button.currentState == LOW) {
                button.pressStartTime = millis();
                button.longPressHandled = false;
            } else {
                if (!button.longPressHandled && (millis() - button.pressStartTime) < BUTTON_LONG_PRESS_MS) {
                    stopMotor();
                }
            }
        }
        
        if (button.currentState == LOW && !button.longPressHandled && 
            (millis() - button.pressStartTime) >= BUTTON_LONG_PRESS_MS) {
            button.longPressHandled = true;
            if (motorDirection == MOTOR_EXTENDING) {
                targetPosition = 100.0;
            } else {
                targetPosition = 0.0;
            }
            startMotor(motorDirection);
        }
    }
    
    button.lastState = reading;
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.print("MQTT message received [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);
    
    if (strcmp(topic, mqttCommandTopic) == 0) {
        if (strcmp(message, "OPEN") == 0) {
            moveToPosition(100.0);
        } else if (strcmp(message, "CLOSE") == 0) {
            moveToPosition(0.0);
        } else if (strcmp(message, "STOP") == 0) {
            stopMotor();
        }
    } else if (strcmp(topic, mqttSetPositionTopic) == 0) {
        float position = atof(message);
        moveToPosition(position);
    } else if (strcmp(topic, mqttCalibrateTopic) == 0) {
        unsigned long newTravelTime = atol(message);
        if (newTravelTime >= 5000 && newTravelTime <= 300000) {
            travelTimeMs = newTravelTime;
            saveToEEPROM();
            Serial.print("Travel time calibrated to: ");
            Serial.print(travelTimeMs);
            Serial.println(" ms");
        }
    } else if (strcmp(topic, mqttSetWindThresholdTopic) == 0) {
        float threshold = atof(message);
        if (threshold >= 0.0 && threshold <= 100.0) {
            windSpeedThreshold = threshold;
            saveToEEPROM();
            publishState();
            Serial.print("Wind threshold set to: ");
            Serial.println(windSpeedThreshold);
        }
    } else if (strcmp(topic, mqttSetWindFactorTopic) == 0) {
        float factor = atof(message);
        if (factor >= 0.1 && factor <= 10.0) {
            windConversionFactor = factor;
            saveToEEPROM();
            publishState();
            Serial.print("Wind conversion factor set to: ");
            Serial.println(windConversionFactor);
        }
    }
}

void reconnectMqtt() {
    if (!WiFi.isConnected() || mqttClient.connected()) return;
    
    unsigned long now = millis();
    if (now - lastMqttReconnect < MQTT_RECONNECT_INTERVAL_MS) return;
    lastMqttReconnect = now;
    
    Serial.print("Attempting MQTT connection...");
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, 
                          mqttAvailabilityTopic, 0, true, "offline")) {
        Serial.println("connected");
        
        mqttClient.publish(mqttAvailabilityTopic, "online", true);
        
        mqttClient.subscribe(mqttCommandTopic);
        mqttClient.subscribe(mqttSetPositionTopic);
        mqttClient.subscribe(mqttCalibrateTopic);
        mqttClient.subscribe(mqttSetWindThresholdTopic);
        mqttClient.subscribe(mqttSetWindFactorTopic);
        
        publishState();
    } else {
        Serial.print("failed, rc=");
        Serial.println(mqttClient.state());
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nESP8266 Awning Controller Starting...");
    
    setupPins();
    loadFromEEPROM();
    
    Serial.print("Initial position: ");
    Serial.print(currentPosition);
    Serial.println("%");
    
    setupWiFi();
    
    buildMqttTopics();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);
    
    if (WiFi.isConnected()) {
        reconnectMqtt();
    }
    
    Serial.println("Setup complete!");
}

void loop() {
    handleButton(extendButton, BUTTON_EXTEND, MOTOR_EXTENDING);
    handleButton(retractButton, BUTTON_RETRACT, MOTOR_RETRACTING);
    
    updatePosition();
    updateWindSpeed();
    checkWindSafety();
    
    if (WiFi.isConnected()) {
        if (!mqttClient.connected()) {
            reconnectMqtt();
        } else {
            mqttClient.loop();
        }
    }
    
    yield();
}