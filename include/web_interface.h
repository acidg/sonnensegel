#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "config_manager.h"

class WebInterface {
private:
    ESP8266WebServer server;
    ConfigManager* configManager;
    bool calibrationInProgress;
    unsigned long calibrationStartTime;
    
    void handleRoot();
    void handleControl();
    void handleStatus();
    void handleCalibrate();
    void handleWindConfig();
    void handleSystemConfig();
    void handleSystemConfigSave();
    void handleFactoryReset();
    void handleNotFound();
    
    String getStatusJson();
    
public:
    WebInterface(ConfigManager* config);
    void begin();
    void loop();
    bool isRunning() const;
};

#endif // WEB_INTERFACE_H