#include "web_interface.h"
#include <ArduinoJson.h>
#include "motor_controller.h"
#include "position_tracker.h"
#include "wind_sensor.h"
#include "constants.h"
#include "web_pages.h"

// External references to global objects from main.cpp
extern MotorController motor;
extern PositionTracker positionTracker;
extern WindSensor windSensor;
extern void saveSettings();
extern void setTargetPositionWithInterrupt(float targetPosition, const char* source);

WebInterface::WebInterface(ConfigManager* config) : server(80), configManager(config), 
    calibrationInProgress(false), calibrationStartTime(0) {
}

void WebInterface::begin() {
    if (!WiFi.isConnected()) {
        Serial.println("Web Interface: WiFi not connected, skipping web server setup");
        return;
    }
    
    // Setup routes
    server.on("/", [this](){ handleRoot(); });
    server.on("/control", HTTP_POST, [this](){ handleControl(); });
    server.on("/status", HTTP_GET, [this](){ handleStatus(); });
    server.on("/calibrate", HTTP_POST, [this](){ handleCalibrate(); });
    server.on("/wind-config", HTTP_POST, [this](){ handleWindConfig(); });
    server.on("/system-config", HTTP_GET, [this](){ handleSystemConfig(); });
    server.on("/system-config", HTTP_POST, [this](){ handleSystemConfigSave(); });
    server.on("/factory-reset", HTTP_POST, [this](){ handleFactoryReset(); });
    server.onNotFound([this](){ handleNotFound(); });
    
    server.begin();
    Serial.print("Web Interface: Started on http://");
    Serial.println(WiFi.localIP());
}

void WebInterface::loop() {
    server.handleClient();
}

bool WebInterface::isRunning() const {
    return WiFi.isConnected();
}

void WebInterface::handleRoot() {
    server.send_P(200, "text/html", HTML_INDEX);
}

void WebInterface::handleControl() {
    if (!server.hasArg("action")) {
        server.send(400, "text/plain", "Missing action parameter");
        return;
    }
    
    String action = server.arg("action");
    
    if (action == "open") {
        setTargetPositionWithInterrupt(100.0, "Web");
    } else if (action == "close") {
        setTargetPositionWithInterrupt(0.0, "Web");
    } else if (action == "stop") {
        motor.stop();
        positionTracker.setTargetPosition(positionTracker.getCurrentPosition());
        Serial.println("Web: Command STOP");
    } else if (action == "position" && server.hasArg("value")) {
        float position = server.arg("value").toFloat();
        if (position >= 0.0 && position <= 100.0) {
            setTargetPositionWithInterrupt(position, "Web");
        } else {
            server.send(400, "text/plain", "Invalid position value");
            return;
        }
    } else {
        server.send(400, "text/plain", "Invalid action");
        return;
    }
    
    server.send(200, "text/plain", "OK");
}

void WebInterface::handleStatus() {
    server.send(200, "application/json", getStatusJson());
}

void WebInterface::handleCalibrate() {
    if (!calibrationInProgress) {
        // Start calibration
        if (positionTracker.getCurrentPosition() > 5.0) {
            server.send(400, "text/plain", "Awning must be at 0% position to start calibration");
            return;
        }
        
        calibrationInProgress = true;
        calibrationStartTime = millis();
        setTargetPositionWithInterrupt(100.0, "Calibration");
        
        Serial.println("Web: Calibration started - awning extending");
        server.send(200, "text/plain", "Calibration started");
    } else {
        // Stop calibration and calculate travel time
        unsigned long travelTime = millis() - calibrationStartTime;
        
        // Stop motor immediately
        motor.stop();
        positionTracker.setTargetPosition(positionTracker.getCurrentPosition());
        
        // Set the measured travel time
        configManager->setTravelTime(travelTime);
        positionTracker.setTravelTime(travelTime);
        configManager->save();
        
        calibrationInProgress = false;
        
        Serial.print("Web: Calibration completed - travel time set to ");
        Serial.print(travelTime);
        Serial.println(" ms");
        
        server.send(200, "text/plain", "Calibration completed");
    }
}

void WebInterface::handleWindConfig() {
    bool updated = false;
    
    if (server.hasArg("threshold")) {
        unsigned long threshold = server.arg("threshold").toInt();
        if (threshold >= MIN_WIND_PULSE_THRESHOLD && threshold <= MAX_WIND_PULSE_THRESHOLD) {
            configManager->setWindThreshold(threshold);
            windSensor.setThreshold(threshold);
            updated = true;
            Serial.print("Web: Wind threshold set to ");
            Serial.print(threshold);
            Serial.println(" pulses/min");
        }
    }
    
    if (updated) {
        configManager->save();
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "No valid parameters provided");
    }
}

void WebInterface::handleSystemConfig() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>System Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        h1 { color: #333; text-align: center; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .form-group { margin: 10px 0; }
        label { display: inline-block; width: 150px; font-weight: bold; }
        input[type="text"], input[type="password"], input[type="number"] { 
            width: 200px; padding: 5px; border: 1px solid #ddd; border-radius: 4px; 
        }
        button { 
            background: #FF9800; color: white; padding: 8px 16px; 
            border: none; border-radius: 4px; cursor: pointer; margin: 5px; 
        }
        button:hover { background: #F57C00; }
        .nav { text-align: center; margin-bottom: 20px; }
        .nav a { margin: 0 10px; color: #2196F3; text-decoration: none; }
    </style>
</head>
<body>
    <div class="container">
        <div class="nav">
            <a href="/">Back to Control</a>
        </div>
        
        <h1>System Configuration</h1>
        
        <form method="POST" action="/system-config">
            <div class="section">
                <h3>WiFi Settings</h3>
                <div class="form-group">
                    <label>WiFi SSID:</label>
                    <input type="text" name="wifi_ssid" value=")rawliteral" + 
                    String(configManager->getWiFiSSID()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>WiFi Password:</label>
                    <input type="password" name="wifi_password" placeholder="Enter new password">
                </div>
            </div>
            
            <div class="section">
                <h3>MQTT Settings</h3>
                <div class="form-group">
                    <label>
                        <input type="checkbox" name="mqtt_enabled" value="1" )rawliteral" + 
                        String(configManager->isMQTTEnabled() ? "checked" : "") + R"rawliteral(> 
                        Enable MQTT Integration
                    </label>
                </div>
                <div class="form-group">
                    <label>MQTT Server:</label>
                    <input type="text" name="mqtt_server" value=")rawliteral" + 
                    String(configManager->getMQTTServer()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>MQTT Port:</label>
                    <input type="number" name="mqtt_port" value=")rawliteral" + 
                    String(configManager->getMQTTPort()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>MQTT Username:</label>
                    <input type="text" name="mqtt_username" value=")rawliteral" + 
                    String(configManager->getMQTTUsername()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>MQTT Password:</label>
                    <input type="password" name="mqtt_password" placeholder="Enter new password">
                </div>
                <div class="form-group">
                    <label>Client ID:</label>
                    <input type="text" name="mqtt_client_id" value=")rawliteral" + 
                    String(configManager->getMQTTClientId()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>Base Topic:</label>
                    <input type="text" name="mqtt_base_topic" value=")rawliteral" + 
                    String(configManager->getMQTTBaseTopic()) + R"rawliteral(">
                </div>
            </div>
            
            <div style="text-align: center;">
                <button type="submit">Save Configuration</button>
            </div>
        </form>
        
        <div style="text-align: center; margin-top: 30px; padding-top: 20px; border-top: 1px solid #ddd;">
            <h3 style="color: #f44336;">Danger Zone</h3>
            <p style="font-size: 14px; color: #666; margin: 10px 0;">
                Factory reset will erase all WiFi, MQTT, and awning settings. The device will restart.
            </p>
            <button type="button" onclick="factoryReset()" 
                    style="background: #f44336; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer;">
                Factory Reset
            </button>
        </div>
    </div>
    
    <script>
        function factoryReset() {
            if (confirm('WARNING: This will erase ALL settings and restart the device.\\n\\nAre you sure you want to continue?')) {
                if (confirm('This action cannot be undone. Continue with factory reset?')) {
                    fetch('/factory-reset', { method: 'POST' })
                        .then(response => {
                            if (response.ok) {
                                alert('Factory reset initiated. Device will restart in a few seconds...');
                                setTimeout(() => {
                                    window.location.href = '/';
                                }, 3000);
                            } else {
                                alert('Factory reset failed. Please try again.');
                            }
                        })
                        .catch(error => {
                            console.error('Reset failed:', error);
                            alert('Factory reset failed. Please try again.');
                        });
                }
            }
        }
    </script>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}

void WebInterface::handleSystemConfigSave() {
    bool wifiChanged = false;
    bool mqttChanged = false;
    
    // Handle WiFi settings
    if (server.hasArg("wifi_ssid")) {
        String currentSSID = configManager->getWiFiSSID();
        String newSSID = server.arg("wifi_ssid");
        String newPassword = server.arg("wifi_password");
        
        if (newSSID != currentSSID || newPassword.length() > 0) {
            const char* password = newPassword.length() > 0 ? newPassword.c_str() : configManager->getWiFiPassword();
            configManager->setWiFiCredentials(newSSID.c_str(), password);
            wifiChanged = true;
        }
    }
    
    // Handle MQTT settings
    if (server.hasArg("mqtt_server")) {
        // Check if MQTT is enabled
        bool mqttEnabled = server.hasArg("mqtt_enabled");
        configManager->setMQTTEnabled(mqttEnabled);
        
        String server_addr = server.arg("mqtt_server");
        uint16_t port = server.arg("mqtt_port").toInt();
        String username = server.arg("mqtt_username");
        String password = server.arg("mqtt_password");
        String clientId = server.arg("mqtt_client_id");
        String baseTopic = server.arg("mqtt_base_topic");
        
        const char* mqttPassword = password.length() > 0 ? password.c_str() : configManager->getMQTTPassword();
        
        configManager->setMQTTConfig(server_addr.c_str(), port, username.c_str(), 
                                   mqttPassword, clientId.c_str(), baseTopic.c_str());
        mqttChanged = true;
    }
    
    // Save configuration
    bool success = configManager->save();
    
    String response = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Configuration Updated</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="3;url=/">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Configuration Updated!</h1>
        <p>)rawliteral";
    
    if (success) {
        response += "Settings have been saved successfully.";
        if (wifiChanged) {
            response += "<br><strong>Note:</strong> WiFi settings changed. Device may restart to apply new settings.";
        }
        if (mqttChanged) {
            response += "<br><strong>Note:</strong> MQTT settings changed. Connection will be reestablished.";
        }
    } else {
        response += "Error saving configuration. Please try again.";
    }
    
    response += R"rawliteral(</p>
        <p>Redirecting to main page...</p>
        <a href="/">Return to Control Panel</a>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", response);
}

void WebInterface::handleFactoryReset() {
    Serial.println("Web: Factory reset requested");
    
    // Reset configuration
    configManager->reset();
    
    String response = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Factory Reset</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; }
        .success { color: #4CAF50; font-size: 1.2em; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Factory Reset Complete</h1>
        <p class="success">All settings have been reset to defaults.</p>
        <p>The device will restart momentarily...</p>
        <p><strong>Note:</strong> After restart, connect to "Sonnensegel" WiFi network to reconfigure.</p>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", response);
    
    // Restart device after a short delay
    delay(2000);
    ESP.restart();
}

void WebInterface::handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}


String WebInterface::getStatusJson() {
    DynamicJsonDocument doc(512);
    
    doc["position"] = positionTracker.getCurrentPosition();
    doc["target"] = positionTracker.getTargetPosition();
    doc["travelTime"] = positionTracker.getTravelTime();
    doc["windPulses"] = windSensor.getPulsesPerMinute();
    doc["windThreshold"] = windSensor.getThreshold();
    doc["calibrating"] = calibrationInProgress;
    
    // Motor state as string
    switch (motor.getState()) {
        case MOTOR_EXTENDING: doc["motor"] = "Extending"; break;
        case MOTOR_RETRACTING: doc["motor"] = "Retracting"; break;
        default: doc["motor"] = "Idle"; break;
    }
    
    String result;
    serializeJson(doc, result);
    return result;
}