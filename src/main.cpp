#include <ESP8266WiFi.h>
#include "pins.h"
#include "constants.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "button_handler.h"
#include "motor_controller.h"
#include "position_tracker.h"
#include "wind_sensor.h"
#include "mqtt_handler.h"
#include "storage.h"
#include "web_interface.h"

// Global objects
ConfigManager configManager;
WiFiManager wifiManager(&configManager);
ButtonHandler extendButton(BUTTON_EXTEND);
ButtonHandler retractButton(BUTTON_RETRACT);
MotorController motor;
PositionTracker positionTracker;
volatile unsigned long windPulseCount = 0;
WindSensor windSensor(&windPulseCount);
MqttHandler mqtt;
Storage storage;
WebInterface webInterface(&configManager);

// Initialize configuration
void initializeConfig() {
    if (!configManager.begin()) {
        Serial.println("Config: Using defaults");
    }
    
    // Initialize WiFi manager
    wifiManager.begin();
}

// Wind sensor ISR
void IRAM_ATTR windSensorISR() {
    windPulseCount++;
}

// Helper function to set new target (allows motor controller to handle direction changes)
void setTargetPositionWithInterrupt(float targetPosition, const char* source = "") {
    // Set new target position - motor control logic will handle direction changes
    positionTracker.setTargetPosition(targetPosition);
    
    if (strlen(source) > 0) {
        Serial.print(source);
        Serial.print(": Target set to ");
        Serial.print(targetPosition);
        Serial.println("%");
    }
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

// Load settings from configuration
void loadSettings() {
    positionTracker.setCurrentPosition(configManager.getCurrentPosition());
    positionTracker.setTargetPosition(configManager.getTargetPosition());
    positionTracker.setTravelTime(configManager.getTravelTime());
    windSensor.setThreshold(configManager.getWindThreshold());
    
    Serial.print("Loaded - Position: ");
    Serial.print(configManager.getCurrentPosition());
    Serial.print("%, Travel time: ");
    Serial.print(configManager.getTravelTime());
    Serial.print("ms, Wind threshold: ");
    Serial.print(configManager.getWindThreshold());
    Serial.println(" pulses/min");
}

// Save current settings
void saveSettings() {
    configManager.setCurrentPosition(positionTracker.getCurrentPosition());
    configManager.setTargetPosition(positionTracker.getTargetPosition());
    configManager.save();
}

// Setup MQTT callbacks
void setupMqttCallbacks() {
    mqtt.onCommand = [](const char* command) {
        if (strcmp(command, "OPEN") == 0) {
            setTargetPositionWithInterrupt(100.0, "MQTT");
        } else if (strcmp(command, "CLOSE") == 0) {
            setTargetPositionWithInterrupt(0.0, "MQTT");
        } else if (strcmp(command, "STOP") == 0) {
            motor.stop();
            positionTracker.setTargetPosition(positionTracker.getCurrentPosition());
            Serial.println("MQTT: Emergency stop");
        }
    };
    
    mqtt.onSetPosition = [](float position) {
        setTargetPositionWithInterrupt(position, "MQTT");
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
        setTargetPositionWithInterrupt(targetPosition, "Button");
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
    
    MotorState requiredDirection = positionTracker.getRequiredDirection();
    
    // Start motor if needed
    if (requiredDirection != MOTOR_IDLE && requiredDirection != motor.getState()) {
        if (motor.isMoving()) {
            // Direction change - no stop pulse needed
            motor.startWithoutStop(requiredDirection);
        } else {
            // Starting from idle - use normal start
            motor.start(requiredDirection);
        }
    }
}

// Handle wind safety
void handleWindSafety() {
    windSensor.update();
    
    if (windSensor.isSafetyTriggered() && positionTracker.getCurrentPosition() > 0.0) {
        setTargetPositionWithInterrupt(0.0, "Wind Safety");
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
    
    initializeConfig();
    initializeComponents();
    loadSettings();
    
    setupMqttCallbacks();
    
    Serial.println("Setup complete!");
}

void loop() {
    // Update WiFi manager (handles connection, fallback, config portal)
    wifiManager.update();
    
    // Initialize services when WiFi becomes available
    static bool servicesInitialized = false;
    if (wifiManager.isConnected() && !servicesInitialized) {
        mqtt.begin(configManager.getMQTTServer(), configManager.getMQTTPort(), 
                  configManager.getMQTTUsername(), configManager.getMQTTPassword(),
                  configManager.getMQTTClientId());
        mqtt.setBaseTopic(configManager.getMQTTBaseTopic());
        webInterface.begin();
        servicesInitialized = true;
        Serial.println("Network services initialized");
    } else if (!wifiManager.isConnected() && servicesInitialized) {
        servicesInitialized = false;
        Serial.println("Network services stopped");
    }
    
    // Handle buttons FIRST - they have priority over all other commands
    bool buttonPressed = false;
    buttonPressed |= handleButtonPress(extendButton, 100.0);
    buttonPressed |= handleButtonPress(retractButton, 0.0);
    
    // Skip motor control updates if button was just pressed to avoid conflicts
    if (!buttonPressed) {
        updateMotorControl();
    }
    
    handleWindSafety();
    
    // Only run network services if connected
    if (servicesInitialized) {
        mqtt.loop();
        webInterface.loop();
        publishState();
    }
    
    yield();
}
