#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "config_manager.h"

enum AwningWiFiMode {
    AWNING_WIFI_CONNECTING,
    AWNING_WIFI_CONNECTED,
    AWNING_WIFI_AP_FALLBACK,
    AWNING_WIFI_FAILED
};

class WiFiManager {
private:
    ConfigManager* configManager;
    ESP8266WebServer* configServer;
    DNSServer* dnsServer;
    AwningWiFiMode currentMode;
    unsigned long lastConnectionAttempt;
    unsigned long lastStatusCheck;
    int connectionAttempts;
    bool apStarted;
    bool hasRetriedFromAP;
    
    static const unsigned long CONNECTION_TIMEOUT = 10000;
    static const unsigned long RETRY_INTERVAL = 60000;
    static const unsigned long STATUS_CHECK_INTERVAL = 5000;
    static const int MAX_CONNECTION_ATTEMPTS = 1;
    static const char* AP_SSID;
    static const char* AP_PASSWORD;
    static const byte DNS_PORT = 53;
    
    void startAP();
    void stopAP();
    bool connectToWiFi();
    void handleConfigPortal();
    void setupConfigServer();
    
public:
    WiFiManager(ConfigManager* config);
    ~WiFiManager();
    
    bool begin();
    void update();
    
    AwningWiFiMode getMode() const { return currentMode; }
    bool isConnected() const { return currentMode == AWNING_WIFI_CONNECTED; }
    bool isInAPMode() const { return currentMode == AWNING_WIFI_AP_FALLBACK; }
    String getLocalIP() const;
    String getAPIP() const;
    
    // Configuration portal handlers
    void handleConfigRoot();
    void handleConfigSave();
    void handleConfigStatus();
    void handleWiFiScan();
    void handleFactoryReset();
    void handleCaptivePortal();
    bool isCaptivePortalRequest();
};

#endif // WIFI_MANAGER_H