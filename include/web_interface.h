#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

class WebInterface {
private:
    ESP8266WebServer server;
    
    void handleRoot();
    void handleControl();
    void handleStatus();
    void handleCalibrate();
    void handleWindConfig();
    void handleNotFound();
    
    String getStatusJson();
    
public:
    WebInterface();
    void begin();
    void loop();
    bool isRunning() const;
};

#endif // WEB_INTERFACE_H