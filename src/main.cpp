#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "pins.h"
#include "constants.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "button_handler.h"
#include "motor_controller.h"
#include "position_tracker.h"
#include "awning_controller.h"
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
AwningController awning(motor, positionTracker);
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
    static unsigned long lastPulseTime = 0;
    unsigned long now = millis();
    
    // Debounce: only count if enough time has passed since last pulse
    if (now - lastPulseTime >= WIND_SENSOR_DEBOUNCE_MS) {
        windPulseCount++;
        lastPulseTime = now;
    }
}

// Helper function to set new target via the state machine
void setTargetPosition(float targetPosition, const char* source = "") {
    awning.setTarget(targetPosition);

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
    float currentPos = configManager.getCurrentPosition();
    // Initialize awning controller with current position (state starts as IDLE)
    awning.setCurrentPosition(currentPos);
    positionTracker.setTravelTime(configManager.getTravelTime());
    windSensor.setThreshold(configManager.getWindThreshold());

    Serial.print("Loaded - Position: ");
    Serial.print(currentPos);
    Serial.print("%, Travel time: ");
    Serial.print(configManager.getTravelTime());
    Serial.print("ms, Wind threshold: ");
    Serial.print(configManager.getWindThreshold());
    Serial.println(" pulses/min");
}

// Save current settings
void saveSettings() {
    configManager.setCurrentPosition(awning.getCurrentPosition());
    configManager.setTargetPosition(awning.getTargetPosition());
    configManager.save();
}

// Setup MQTT callbacks
void setupMqttCallbacks() {
    mqtt.onCommand = [](const char* command) {
        if (strcmp(command, "OPEN") == 0) {
            setTargetPosition(100.0, "MQTT");
        } else if (strcmp(command, "CLOSE") == 0) {
            setTargetPosition(0.0, "MQTT");
        } else if (strcmp(command, "STOP") == 0) {
            awning.stopBoth();
            saveSettings();
            Serial.println("MQTT: Stop (both relays)");
        }
    };

    mqtt.onSetPosition = [](float position) {
        setTargetPosition(position, "MQTT");
    };

    mqtt.onSetWindThreshold = [](float threshold) {
        windSensor.setThreshold((unsigned long)threshold);
        saveSettings();
        Serial.print("Wind threshold set to: ");
        Serial.print((unsigned long)threshold);
        Serial.println(" pulses/min");
    };
}

// Handle extend button - returns true if button was pressed
bool handleExtendButton() {
    ButtonAction action = extendButton.update();

    if (action == BUTTON_SHORT_PRESS) {
        awning.stop(RELAY_EXTEND);
        saveSettings();
        Serial.println("Button: Stop (extend relay)");
        return true;
    }
    if (action == BUTTON_LONG_PRESS) {
        setTargetPosition(100.0, "Button");
        return true;
    }
    return false;
}

// Handle retract button - returns true if button was pressed
bool handleRetractButton() {
    ButtonAction action = retractButton.update();

    if (action == BUTTON_SHORT_PRESS) {
        awning.stop(RELAY_RETRACT);
        saveSettings();
        Serial.println("Button: Stop (retract relay)");
        return true;
    }
    if (action == BUTTON_LONG_PRESS) {
        setTargetPosition(0.0, "Button");
        return true;
    }
    return false;
}

// Handle wind safety
void handleWindSafety() {
    windSensor.update();

    if (windSensor.isSafetyTriggered() && awning.getCurrentPosition() > 0.0) {
        setTargetPosition(0.0, "Wind Safety");
        windSensor.resetSafetyTrigger();
    }
}

// Convert AwningState to MotorState for MQTT publishing
MotorState awningStateToMotorState(AwningState state) {
    switch (state) {
        case AWNING_EXTENDING: return MOTOR_EXTENDING;
        case AWNING_RETRACTING: return MOTOR_RETRACTING;
        default: return MOTOR_IDLE;
    }
}

// Publish state periodically
void publishState() {
    static unsigned long lastPublish = 0;
    unsigned long now = millis();

    if (now - lastPublish >= 5000) {
        mqtt.publishState(awningStateToMotorState(awning.getState()), awning.getCurrentPosition());
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
    static bool mqttInitialized = false;
    
    if (wifiManager.isConnected() && !servicesInitialized) {
        // Start mDNS service
        const char* hostname = configManager.getHostname();
        if (MDNS.begin(hostname)) {
            Serial.printf("mDNS: Started with hostname '%s.local'\n", hostname);
            
            // Add service announcements
            MDNS.addService("http", "tcp", 80);
            MDNS.addServiceTxt("http", "tcp", "device", "sonnensegel");
            MDNS.addServiceTxt("http", "tcp", "version", "1.0");
            Serial.println("mDNS: HTTP service announced");
        } else {
            Serial.println("mDNS: Failed to start");
        }
        
        webInterface.begin();
        servicesInitialized = true;
        Serial.println("Web interface initialized");
        
        // Initialize MQTT only if enabled
        if (configManager.isMQTTEnabled() && !mqttInitialized) {
            mqtt.begin(configManager.getMQTTServer(), configManager.getMQTTPort(), 
                      configManager.getMQTTUsername(), configManager.getMQTTPassword(),
                      configManager.getMQTTClientId());
            mqtt.setBaseTopic(configManager.getMQTTBaseTopic());
            mqttInitialized = true;
            Serial.println("MQTT service initialized");
        }
    } else if (!wifiManager.isConnected() && servicesInitialized) {
        MDNS.end();
        servicesInitialized = false;
        mqttInitialized = false;
        Serial.println("Network services stopped");
    }
    
    // Handle MQTT enable/disable changes
    if (wifiManager.isConnected() && servicesInitialized) {
        if (configManager.isMQTTEnabled() && !mqttInitialized) {
            mqtt.begin(configManager.getMQTTServer(), configManager.getMQTTPort(), 
                      configManager.getMQTTUsername(), configManager.getMQTTPassword(),
                      configManager.getMQTTClientId());
            mqtt.setBaseTopic(configManager.getMQTTBaseTopic());
            mqttInitialized = true;
            Serial.println("MQTT service enabled");
        } else if (!configManager.isMQTTEnabled() && mqttInitialized) {
            mqttInitialized = false;
            Serial.println("MQTT service disabled");
        }
    }
    
    // Handle buttons FIRST - they have priority over all other commands
    bool buttonPressed = handleExtendButton() || handleRetractButton();

    // Update awning state machine (handles motor control)
    static bool wasMoving = false;
    if (!buttonPressed) {
        awning.update();
        // Save settings when motor stops
        bool isMoving = awning.isMoving();
        if (wasMoving && !isMoving) {
            saveSettings();
        }
        wasMoving = isMoving;
    }

    handleWindSafety();
    
    // Only run network services if connected
    if (servicesInitialized) {
        MDNS.update();
        webInterface.loop();
        
        // Only run MQTT services if enabled and initialized
        if (mqttInitialized) {
            mqtt.loop();
            publishState();
        }
    }
    
    yield();
}
