#include "wifi_manager.h"

const char* WiFiManager::AP_SSID = "Awning-Config";
const char* WiFiManager::AP_PASSWORD = "configure";

WiFiManager::WiFiManager(ConfigManager* config) 
    : configManager(config), configServer(nullptr), currentMode(AWNING_WIFI_CONNECTING),
      lastConnectionAttempt(0), lastStatusCheck(0), connectionAttempts(0), apStarted(false) {
}

WiFiManager::~WiFiManager() {
    if (configServer) {
        delete configServer;
    }
}

bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    
    // If no WiFi config, start AP immediately
    if (!configManager->hasWiFiConfig()) {
        Serial.println("WiFi: No configuration found, starting AP");
        startAP();
        return true;
    }
    
    // Try to connect to configured WiFi
    Serial.println("WiFi: Attempting connection to configured network");
    return connectToWiFi();
}

bool WiFiManager::connectToWiFi() {
    const char* ssid = configManager->getWiFiSSID();
    const char* password = configManager->getWiFiPassword();
    
    if (strlen(ssid) == 0) {
        return false;
    }
    
    Serial.printf("WiFi: Connecting to '%s'\n", ssid);
    WiFi.begin(ssid, password);
    
    currentMode = AWNING_WIFI_CONNECTING;
    lastConnectionAttempt = millis();
    connectionAttempts++;
    
    return true;
}

void WiFiManager::startAP() {
    if (apStarted) {
        return;
    }
    
    WiFi.mode(WIFI_AP_STA);
    
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMask(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMask);
    
    bool success = WiFi.softAP(AP_SSID, AP_PASSWORD);
    if (success) {
        Serial.printf("WiFi: AP started - SSID: %s, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
        setupConfigServer();
        currentMode = AWNING_WIFI_AP_FALLBACK;
        apStarted = true;
    } else {
        Serial.println("WiFi: Failed to start AP");
        currentMode = AWNING_WIFI_FAILED;
    }
}

void WiFiManager::stopAP() {
    if (!apStarted) {
        return;
    }
    
    if (configServer) {
        delete configServer;
        configServer = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apStarted = false;
    Serial.println("WiFi: AP stopped");
}

void WiFiManager::setupConfigServer() {
    if (configServer) {
        delete configServer;
    }
    
    configServer = new ESP8266WebServer(80);
    
    configServer->on("/", [this]() { handleConfigRoot(); });
    configServer->on("/save", HTTP_POST, [this]() { handleConfigSave(); });
    configServer->on("/status", [this]() { handleConfigStatus(); });
    
    configServer->begin();
    Serial.println("WiFi: Configuration server started on port 80");
}

void WiFiManager::update() {
    unsigned long now = millis();
    
    // Handle config server if in AP mode
    if (configServer && currentMode == AWNING_WIFI_AP_FALLBACK) {
        configServer->handleClient();
    }
    
    // Check connection status periodically
    if (now - lastStatusCheck >= STATUS_CHECK_INTERVAL) {
        lastStatusCheck = now;
        
        if (currentMode == AWNING_WIFI_CONNECTING) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("WiFi: Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                currentMode = AWNING_WIFI_CONNECTED;
                connectionAttempts = 0;
                
                // Stop AP if it was running
                if (apStarted) {
                    stopAP();
                }
            } else if (now - lastConnectionAttempt >= CONNECTION_TIMEOUT) {
                Serial.println("WiFi: Connection timeout");
                
                if (connectionAttempts >= MAX_CONNECTION_ATTEMPTS) {
                    Serial.println("WiFi: Max attempts reached, starting AP fallback");
                    startAP();
                } else {
                    Serial.println("WiFi: Retrying connection");
                    connectToWiFi();
                }
            }
        } else if (currentMode == AWNING_WIFI_CONNECTED) {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi: Connection lost, attempting reconnection");
                currentMode = AWNING_WIFI_CONNECTING;
                connectionAttempts = 0;
                connectToWiFi();
            }
        } else if (currentMode == AWNING_WIFI_AP_FALLBACK && configManager->hasWiFiConfig()) {
            // Periodically try to reconnect if we have WiFi config
            if (now - lastConnectionAttempt >= RETRY_INTERVAL) {
                Serial.println("WiFi: Attempting reconnection from AP mode");
                connectToWiFi();
            }
        }
    }
}

String WiFiManager::getLocalIP() const {
    if (currentMode == AWNING_WIFI_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "";
}

String WiFiManager::getAPIP() const {
    if (currentMode == AWNING_WIFI_AP_FALLBACK) {
        return WiFi.softAPIP().toString();
    }
    return "";
}

void WiFiManager::handleConfigRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Awning Controller Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }
        h1 { color: #333; text-align: center; }
        .form-group { margin: 15px 0; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input[type="text"], input[type="password"], input[type="number"] { 
            width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; 
        }
        button { 
            background: #4CAF50; color: white; padding: 10px 20px; 
            border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 16px; 
        }
        button:hover { background: #45a049; }
        .section { margin: 20px 0; padding: 15px; border: 1px solid #ddd; border-radius: 5px; }
        .info { background: #e3f2fd; padding: 10px; border-radius: 4px; margin: 10px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Awning Controller Setup</h1>
        
        <div class="info">
            Connect to your WiFi network and configure MQTT settings.
        </div>
        
        <form method="POST" action="/save">
            <div class="section">
                <h3>WiFi Configuration</h3>
                <div class="form-group">
                    <label>WiFi Network (SSID):</label>
                    <input type="text" name="wifi_ssid" value=")rawliteral" + 
                    String(configManager->getWiFiSSID()) + R"rawliteral(" required>
                </div>
                <div class="form-group">
                    <label>WiFi Password:</label>
                    <input type="password" name="wifi_password" value="">
                </div>
            </div>
            
            <div class="section">
                <h3>MQTT Configuration</h3>
                <div class="form-group">
                    <label>MQTT Server:</label>
                    <input type="text" name="mqtt_server" value=")rawliteral" + 
                    String(configManager->getMQTTServer()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>MQTT Port:</label>
                    <input type="number" name="mqtt_port" value=")rawliteral" + 
                    String(configManager->getMQTTPort()) + R"rawliteral(" min="1" max="65535">
                </div>
                <div class="form-group">
                    <label>MQTT Username (optional):</label>
                    <input type="text" name="mqtt_username" value=")rawliteral" + 
                    String(configManager->getMQTTUsername()) + R"rawliteral(">
                </div>
                <div class="form-group">
                    <label>MQTT Password (optional):</label>
                    <input type="password" name="mqtt_password" value="">
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
            
            <button type="submit">Save Configuration</button>
        </form>
        
        <div style="text-align: center; margin-top: 20px;">
            <a href="/status">Check Status</a>
        </div>
    </div>
</body>
</html>
)rawliteral";
    
    configServer->send(200, "text/html", html);
}

void WiFiManager::handleConfigSave() {
    // Save WiFi configuration
    String ssid = configServer->arg("wifi_ssid");
    String password = configServer->arg("wifi_password");
    
    if (ssid.length() > 0) {
        configManager->setWiFiCredentials(ssid.c_str(), password.c_str());
    }
    
    // Save MQTT configuration
    String mqttServer = configServer->arg("mqtt_server");
    String mqttPort = configServer->arg("mqtt_port");
    String mqttUsername = configServer->arg("mqtt_username");
    String mqttPassword = configServer->arg("mqtt_password");
    String mqttClientId = configServer->arg("mqtt_client_id");
    String mqttBaseTopic = configServer->arg("mqtt_base_topic");
    
    configManager->setMQTTConfig(
        mqttServer.c_str(),
        mqttPort.toInt(),
        mqttUsername.c_str(),
        mqttPassword.c_str(),
        mqttClientId.c_str(),
        mqttBaseTopic.c_str()
    );
    
    // Save to EEPROM
    bool success = configManager->save();
    
    String response = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Configuration Saved</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="5;url=/">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 30px; border-radius: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>)rawliteral";
    
    if (success) {
        response += "Configuration Saved!</h1><p>The device will now attempt to connect to the configured WiFi network.</p>";
    } else {
        response += "Save Failed!</h1><p>There was an error saving the configuration. Please try again.</p>";
    }
    
    response += R"rawliteral(
        <p>Redirecting in 5 seconds...</p>
        <a href="/">Return to Setup</a>
    </div>
</body>
</html>
)rawliteral";
    
    configServer->send(200, "text/html", response);
    
    if (success && ssid.length() > 0) {
        // Attempt to connect to new WiFi configuration
        connectionAttempts = 0;
        connectToWiFi();
    }
}

void WiFiManager::handleConfigStatus() {
    String status;
    String ip;
    
    switch (currentMode) {
        case AWNING_WIFI_CONNECTING:
            status = "Connecting to WiFi...";
            break;
        case AWNING_WIFI_CONNECTED:
            status = "Connected to WiFi";
            ip = WiFi.localIP().toString();
            break;
        case AWNING_WIFI_AP_FALLBACK:
            status = "Access Point Mode (Fallback)";
            ip = WiFi.softAPIP().toString();
            break;
        case AWNING_WIFI_FAILED:
            status = "Connection Failed";
            break;
    }
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>WiFi Status</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="5">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 400px; margin: 50px auto; background: white; padding: 20px; border-radius: 10px; text-align: center; }
    </style>
</head>
<body>
    <div class="container">
        <h1>WiFi Status</h1>
        <p><strong>Status:</strong> )rawliteral" + status + R"rawliteral(</p>)rawliteral";
    
    if (ip.length() > 0) {
        html += "<p><strong>IP Address:</strong> " + ip + "</p>";
    }
    
    html += R"rawliteral(
        <p><strong>SSID:</strong> )rawliteral" + String(configManager->getWiFiSSID()) + R"rawliteral(</p>
        <p>Page refreshes every 5 seconds</p>
        <a href="/">Return to Setup</a>
    </div>
</body>
</html>
)rawliteral";
    
    configServer->send(200, "text/html", html);
}