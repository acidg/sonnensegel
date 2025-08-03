#include "wifi_manager.h"

const char* WiFiManager::AP_SSID = "Sonnensegel";
const char* WiFiManager::AP_PASSWORD = nullptr;

WiFiManager::WiFiManager(ConfigManager* config) 
    : configManager(config), configServer(nullptr), dnsServer(nullptr), currentMode(AWNING_WIFI_CONNECTING),
      lastConnectionAttempt(0), lastStatusCheck(0), connectionAttempts(0), apStarted(false), hasRetriedFromAP(false) {
}

WiFiManager::~WiFiManager() {
    if (configServer) {
        delete configServer;
    }
    if (dnsServer) {
        delete dnsServer;
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
    const char* hostname = configManager->getHostname();
    
    if (strlen(ssid) == 0) {
        return false;
    }
    
    Serial.printf("WiFi: Connecting to '%s'\n", ssid);
    
    // If we're in AP mode, switch to STA+AP mode to keep AP running during connection attempt
    if (currentMode == AWNING_WIFI_AP_FALLBACK) {
        WiFi.mode(WIFI_AP_STA);
    }
    
    // Set hostname before connecting
    if (strlen(hostname) > 0) {
        WiFi.setHostname(hostname);
        Serial.printf("WiFi: Hostname set to '%s'\n", hostname);
    }
    
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
    
    bool success;
    if (AP_PASSWORD) {
        success = WiFi.softAP(AP_SSID, AP_PASSWORD);
    } else {
        success = WiFi.softAP(AP_SSID);
    }
    if (success) {
        Serial.printf("WiFi: AP started - SSID: %s (open), IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
        setupConfigServer();
        
        // Start DNS server for captive portal
        if (!dnsServer) {
            dnsServer = new DNSServer();
        }
        dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
        Serial.println("WiFi: DNS server started for captive portal");
        
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
    
    if (dnsServer) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    apStarted = false;
    Serial.println("WiFi: AP and DNS server stopped");
}

void WiFiManager::setupConfigServer() {
    if (configServer) {
        delete configServer;
    }
    
    configServer = new ESP8266WebServer(80);
    
    configServer->on("/", [this]() { handleConfigRoot(); });
    configServer->on("/save", HTTP_POST, [this]() { handleConfigSave(); });
    configServer->on("/status", [this]() { handleConfigStatus(); });
    configServer->on("/scan", [this]() { handleWiFiScan(); });
    configServer->on("/reset", HTTP_POST, [this]() { handleFactoryReset(); });
    
    // Captive portal handlers - catch all requests
    configServer->onNotFound([this]() { handleCaptivePortal(); });
    
    configServer->begin();
    Serial.println("WiFi: Configuration server started on port 80");
}

void WiFiManager::update() {
    unsigned long now = millis();
    
    // Handle config server and DNS server if in AP mode
    if (currentMode == AWNING_WIFI_AP_FALLBACK) {
        if (dnsServer) {
            dnsServer->processNextRequest();
        }
        if (configServer) {
            configServer->handleClient();
        }
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
                    if (!apStarted) {
                        hasRetriedFromAP = false;  // Reset retry flag when entering AP mode
                        startAP();
                    } else {
                        currentMode = AWNING_WIFI_AP_FALLBACK;
                    }
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
        } else if (currentMode == AWNING_WIFI_AP_FALLBACK && configManager->hasWiFiConfig() && !hasRetriedFromAP) {
            // Try to reconnect once after entering AP mode, then stop retrying
            if (now - lastConnectionAttempt >= RETRY_INTERVAL) {
                Serial.println("WiFi: Attempting final reconnection from AP mode");
                hasRetriedFromAP = true;
                connectionAttempts = 0;
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
        .btn-scan { background: #2196F3; margin-bottom: 10px; }
        .btn-scan:hover { background: #1976D2; }
        .wifi-networks { max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 4px; margin: 10px 0; }
        .wifi-network { 
            padding: 10px; border-bottom: 1px solid #eee; cursor: pointer; display: flex; justify-content: space-between; align-items: center;
        }
        .wifi-network:hover { background: #f5f5f5; }
        .wifi-network:last-child { border-bottom: none; }
        .wifi-ssid { font-weight: bold; }
        .wifi-signal { 
            font-size: 12px; color: #666; display: flex; align-items: center; gap: 5px;
        }
        .signal-bars { font-size: 16px; }
        .wifi-lock { color: #ff9800; }
        .scanning { text-align: center; padding: 20px; color: #666; }
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
                
                <button type="button" class="btn-scan" onclick="scanWiFi()">Scan for Networks</button>
                <div id="wifi-networks" class="wifi-networks" style="display: none;"></div>
                
                <div class="form-group">
                    <label>WiFi Network (SSID):</label>
                    <input type="text" id="wifi_ssid" name="wifi_ssid" value=")rawliteral" + 
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
            <br><br>
            <button type="button" onclick="factoryReset()" style="background: #f44336; width: auto; padding: 8px 16px; font-size: 14px;">
                Factory Reset
            </button>
        </div>
    </div>
    
    <script>
        function scanWiFi() {
            const button = document.querySelector('.btn-scan');
            const networksDiv = document.getElementById('wifi-networks');
            
            button.textContent = 'Scanning...';
            button.disabled = true;
            
            networksDiv.innerHTML = '<div class="scanning">Scanning for WiFi networks...</div>';
            networksDiv.style.display = 'block';
            
            fetch('/scan')
                .then(response => response.json())
                .then(networks => {
                    displayNetworks(networks);
                    button.textContent = 'Scan for Networks';
                    button.disabled = false;
                })
                .catch(error => {
                    console.error('Scan failed:', error);
                    networksDiv.innerHTML = '<div class="scanning">Scan failed. Please try again.</div>';
                    button.textContent = 'Scan for Networks';
                    button.disabled = false;
                });
        }
        
        function displayNetworks(networks) {
            const networksDiv = document.getElementById('wifi-networks');
            
            if (networks.length === 0) {
                networksDiv.innerHTML = '<div class="scanning">No networks found</div>';
                return;
            }
            
            // Sort by signal strength (RSSI)
            networks.sort((a, b) => b.rssi - a.rssi);
            
            let html = '';
            networks.forEach(network => {
                if (network.ssid && network.ssid.trim() !== '') {
                    html += `
                        <div class="wifi-network" onclick="selectNetwork('${escapeHtml(network.ssid)}')">
                            <div class="wifi-ssid">${escapeHtml(network.ssid)}</div>
                            <div class="wifi-signal">
                                <span>${network.rssi} dBm</span>
                            </div>
                        </div>
                    `;
                }
            });
            
            networksDiv.innerHTML = html || '<div class="scanning">No networks found</div>';
        }
        
        function selectNetwork(ssid) {
            document.getElementById('wifi_ssid').value = ssid;
            document.getElementById('wifi-networks').style.display = 'none';
        }
        
        
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }
        
        function factoryReset() {
            if (confirm('Are you sure you want to reset all settings to defaults? This will erase WiFi, MQTT, and awning configuration. The device will restart.')) {
                fetch('/reset', { method: 'POST' })
                    .then(() => {
                        alert('Factory reset initiated. Device will restart...');
                    })
                    .catch(error => {
                        console.error('Reset failed:', error);
                        alert('Reset failed. Please try again.');
                    });
            }
        }
    </script>
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
    bool mqttEnabled = configServer->hasArg("mqtt_enabled");
    configManager->setMQTTEnabled(mqttEnabled);
    
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
        hasRetriedFromAP = false;  // Reset retry flag for new credentials
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

void WiFiManager::handleWiFiScan() {
    Serial.println("WiFi: Starting network scan...");
    
    // Start WiFi scan
    int networkCount = WiFi.scanNetworks();
    
    // Build JSON response
    String json = "[";
    
    if (networkCount > 0) {
        for (int i = 0; i < networkCount; i++) {
            if (i > 0) json += ",";
            
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i));
            json += "}";
        }
    }
    
    json += "]";
    
    Serial.printf("WiFi: Scan complete - found %d networks\n", networkCount);
    
    configServer->sendHeader("Access-Control-Allow-Origin", "*");
    configServer->send(200, "application/json", json);
    
    // Clean up scan results
    WiFi.scanDelete();
}

void WiFiManager::handleFactoryReset() {
    Serial.println("WiFi: Factory reset requested");
    
    // Reset configuration
    configManager->reset();
    
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Factory Reset</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="5;url=/">
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
        <p>The device will restart and create a fresh configuration.</p>
        <p>Redirecting to setup page in 5 seconds...</p>
        <a href="/">Return to Setup</a>
    </div>
</body>
</html>
)rawliteral";
    
    configServer->send(200, "text/html", html);
    
    // Restart device after a short delay
    delay(1000);
    ESP.restart();
}

bool WiFiManager::isCaptivePortalRequest() {
    String host = configServer->hostHeader();
    return !host.equals(WiFi.softAPIP().toString());
}

void WiFiManager::handleCaptivePortal() {
    // If this is a captive portal probe or request for unknown domain, redirect to setup
    if (isCaptivePortalRequest()) {
        String redirectURL = "http://" + WiFi.softAPIP().toString() + "/";
        configServer->sendHeader("Location", redirectURL, true);
        configServer->send(302, "text/plain", "Redirecting to setup...");
        return;
    }
    
    // For direct IP access, show welcome page with redirect to setup
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Awning Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta http-equiv="refresh" content="3;url=/">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            margin: 0; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container { 
            max-width: 400px; 
            background: rgba(255,255,255,0.1); 
            padding: 40px; 
            border-radius: 20px; 
            text-align: center;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.2);
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
        }
        h1 { 
            margin-bottom: 20px; 
            font-size: 2.5em; 
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        p { 
            font-size: 1.2em; 
            margin: 15px 0; 
            line-height: 1.5;
        }
        .wifi-info {
            background: rgba(255,255,255,0.1);
            padding: 15px;
            border-radius: 10px;
            margin: 20px 0;
        }
        .button {
            display: inline-block;
            background: rgba(255,255,255,0.2);
            color: white;
            text-decoration: none;
            padding: 12px 24px;
            border-radius: 25px;
            margin: 10px;
            border: 2px solid rgba(255,255,255,0.3);
            transition: all 0.3s ease;
        }
        .button:hover {
            background: rgba(255,255,255,0.3);
            transform: translateY(-2px);
        }
        .icon { font-size: 3em; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Sonnensegel</h1>
        <p>Welcome to the Awning Controller!</p>
        
        <div class="wifi-info">
            <p><strong>Network:</strong> )rawliteral" + String(AP_SSID) + R"rawliteral(</p>
            <p><strong>IP:</strong> )rawliteral" + WiFi.softAPIP().toString() + R"rawliteral(</p>
        </div>
        
        <p>Configure your WiFi and MQTT settings to get started.</p>
        
        <a href="/" class="button">Setup WiFi & MQTT</a>
        <a href="/status" class="button">Status</a>
        
        <p style="font-size: 0.9em; margin-top: 30px; opacity: 0.8;">
            Redirecting automatically in 3 seconds...
        </p>
    </div>
</body>
</html>
)rawliteral";
    
    configServer->send(200, "text/html", html);
}