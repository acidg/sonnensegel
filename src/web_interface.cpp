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

WebInterface::WebInterface() : server(80) {
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
        positionTracker.setTargetPosition(100.0);
        Serial.println("Web: Command OPEN");
    } else if (action == "close") {
        positionTracker.setTargetPosition(0.0);
        Serial.println("Web: Command CLOSE");
    } else if (action == "stop") {
        motor.stop();
        positionTracker.setTargetPosition(positionTracker.getCurrentPosition());
        Serial.println("Web: Command STOP");
    } else if (action == "position" && server.hasArg("value")) {
        float position = server.arg("value").toFloat();
        if (position >= 0.0 && position <= 100.0) {
            positionTracker.setTargetPosition(position);
            Serial.print("Web: Set position to ");
            Serial.print(position);
            Serial.println("%");
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
    if (!server.hasArg("travelTime")) {
        server.send(400, "text/plain", "Missing travelTime parameter");
        return;
    }
    
    unsigned long travelTime = server.arg("travelTime").toInt();
    if (travelTime < 1000 || travelTime > 120000) {
        server.send(400, "text/plain", "Travel time must be between 1000-120000ms");
        return;
    }
    
    positionTracker.setTravelTime(travelTime);
    saveSettings();
    
    Serial.print("Web: Travel time calibrated to ");
    Serial.print(travelTime);
    Serial.println(" ms");
    
    server.send(200, "text/plain", "OK");
}

void WebInterface::handleWindConfig() {
    bool updated = false;
    
    if (server.hasArg("threshold")) {
        float threshold = server.arg("threshold").toFloat();
        if (threshold >= 0.0 && threshold <= 100.0) {
            windSensor.setThreshold(threshold);
            updated = true;
            Serial.print("Web: Wind threshold set to ");
            Serial.println(threshold);
        }
    }
    
    if (server.hasArg("factor")) {
        float factor = server.arg("factor").toFloat();
        if (factor > 0.0 && factor <= 10.0) {
            windSensor.setConversionFactor(factor);
            updated = true;
            Serial.print("Web: Wind factor set to ");
            Serial.println(factor);
        }
    }
    
    if (updated) {
        saveSettings();
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "No valid parameters provided");
    }
}

void WebInterface::handleNotFound() {
    server.send(404, "text/plain", "Not Found");
}


String WebInterface::getStatusJson() {
    DynamicJsonDocument doc(512);
    
    doc["position"] = positionTracker.getCurrentPosition();
    doc["target"] = positionTracker.getTargetPosition();
    doc["travelTime"] = positionTracker.getTravelTime();
    doc["windSpeed"] = windSensor.getCurrentSpeed();
    doc["windThreshold"] = windSensor.getThreshold();
    doc["windFactor"] = windSensor.getConversionFactor();
    
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