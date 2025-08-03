#include <ESP8266WiFi.h>
#include "config.h"
#include "pins.h"
#include "constants.h"
#include "button_handler.h"
#include "motor_controller.h"
#include "position_tracker.h"
#include "wind_sensor.h"
#include "mqtt_handler.h"
#include "storage.h"
#include "web_interface.h"

// Global objects
ButtonHandler extendButton(BUTTON_EXTEND);
ButtonHandler retractButton(BUTTON_RETRACT);
MotorController motor;
PositionTracker positionTracker;
volatile unsigned long windPulseCount = 0;
WindSensor windSensor(&windPulseCount);
MqttHandler mqtt;
Storage storage;
WebInterface webInterface;

// WiFi setup
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
        Serial.println("\nWiFi connected");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed - continuing without network");
    }
}

// Wind sensor ISR
void IRAM_ATTR windSensorISR() {
    windPulseCount++;
}

// Initialize all components
void initializeComponents() {
    extendButton.begin();
    retractButton.begin();
    motor.begin();
    windSensor.begin();
    storage.begin();
    
    attachInterrupt(digitalPinToInterrupt(WIND_SENSOR_PIN), windSensorISR, FALLING);
}

// Load settings from storage
void loadSettings() {
    StorageData data = storage.load();
    
    positionTracker.setCurrentPosition(data.position);
    positionTracker.setTargetPosition(data.position);
    positionTracker.setTravelTime(data.travelTime);
    windSensor.setThreshold(data.windThreshold);
    
    Serial.print("Loaded - Position: ");
    Serial.print(data.position);
    Serial.print("%, Travel time: ");
    Serial.print(data.travelTime);
    Serial.print("ms, Wind threshold: ");
    Serial.print(data.windThreshold);
    Serial.println(" pulses/min");
}

// Save current settings
void saveSettings() {
    StorageData data = {
        .position = positionTracker.getCurrentPosition(),
        .travelTime = positionTracker.getTravelTime(),
        .windThreshold = windSensor.getThreshold()
    };
    storage.save(data);
}

// Setup MQTT callbacks
void setupMqttCallbacks() {
    mqtt.onCommand = [](const char* command) {
        if (strcmp(command, "OPEN") == 0) {
            positionTracker.setTargetPosition(100.0);
        } else if (strcmp(command, "CLOSE") == 0) {
            positionTracker.setTargetPosition(0.0);
        } else if (strcmp(command, "STOP") == 0) {
            motor.stop();
            positionTracker.setTargetPosition(positionTracker.getCurrentPosition());
        }
    };
    
    mqtt.onSetPosition = [](float position) {
        positionTracker.setTargetPosition(position);
    };
    
    mqtt.onCalibrate = [](unsigned long travelTime) {
        positionTracker.setTravelTime(travelTime);
        saveSettings();
        Serial.print("Travel time calibrated to: ");
        Serial.print(travelTime);
        Serial.println(" ms");
    };
    
    mqtt.onSetWindThreshold = [](float threshold) {
        windSensor.setThreshold((unsigned long)threshold);
        saveSettings();
        Serial.print("Wind threshold set to: ");
        Serial.print((unsigned long)threshold);
        Serial.println(" pulses/min");
    };
}

// Handle button press - returns true if button was pressed (for priority handling)
bool handleButtonPress(ButtonHandler& button, float targetPosition) {
    ButtonAction action = button.update();
    
    if (action == BUTTON_SHORT_PRESS) {
        motor.stop();
        positionTracker.setTargetPosition(positionTracker.getCurrentPosition());
        saveSettings();
        Serial.println("Button: Emergency stop");
        return true;
    } else if (action == BUTTON_LONG_PRESS) {
        positionTracker.setTargetPosition(targetPosition);
        Serial.print("Button: Moving to ");
        Serial.print(targetPosition);
        Serial.println("%");
        return true;
    }
    return false;
}

// Check if motor needs to start/stop
void updateMotorControl() {
    positionTracker.update(motor.getState());
    
    if (motor.isMoving() && positionTracker.shouldStop(motor.getState())) {
        bool stoppingAtLimit = positionTracker.isStoppingAtLimit(motor.getState());
        motor.stop(!stoppingAtLimit);
        saveSettings();
        return;
    }
    
    if (!motor.isMoving() && !positionTracker.shouldStop(motor.getState())) {
        MotorState requiredDirection = positionTracker.getRequiredDirection();
        if (requiredDirection != MOTOR_IDLE) {
            motor.start(requiredDirection);
        }
    }
}

// Handle wind safety
void handleWindSafety() {
    windSensor.update();
    
    if (windSensor.isSafetyTriggered() && positionTracker.getCurrentPosition() > 0.0) {
        positionTracker.setTargetPosition(0.0);
        windSensor.resetSafetyTrigger();
    }
}

// Publish state periodically
void publishState() {
    static unsigned long lastPublish = 0;
    unsigned long now = millis();
    
    if (now - lastPublish >= 5000) {
        mqtt.publishState(motor.getState(), positionTracker.getCurrentPosition());
        mqtt.publishWindData(windSensor.getPulsesPerMinute(), 
                           windSensor.getThreshold());
        lastPublish = now;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nESP8266 Awning Controller Starting...");
    
    initializeComponents();
    loadSettings();
    setupWiFi();
    
    mqtt.begin();
    setupMqttCallbacks();
    
    webInterface.begin();
    
    Serial.println("Setup complete!");
}

void loop() {
    // Handle buttons FIRST - they have priority over all other commands
    bool buttonPressed = false;
    buttonPressed |= handleButtonPress(extendButton, 100.0);
    buttonPressed |= handleButtonPress(retractButton, 0.0);
    
    // Skip motor control updates if button was just pressed to avoid conflicts
    if (!buttonPressed) {
        updateMotorControl();
    }
    
    handleWindSafety();
    
    mqtt.loop();
    webInterface.loop();
    publishState();
    
    yield();
}
