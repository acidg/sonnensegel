#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

struct WiFiConfig {
    char ssid[64];
    char password[64];
};

struct MQTTConfig {
    bool enabled;
    char server[64];
    uint16_t port;
    char username[32];
    char password[64];
    char clientId[32];
    char baseTopic[64];
};

struct AwningConfig {
    unsigned long travelTimeMs;
    unsigned long windThreshold;
    float currentPosition;
    float targetPosition;
};

struct SystemConfig {
    uint32_t magic;
    WiFiConfig wifi;
    MQTTConfig mqtt;
    AwningConfig awning;
    uint32_t checksum;
};

class ConfigManager {
private:
    SystemConfig config;
    bool configValid;
    
    uint32_t calculateChecksum(const SystemConfig& cfg) const;
    void setDefaults();
    
public:
    ConfigManager();
    bool begin();
    bool load();
    bool save();
    void reset();
    
    // WiFi getters/setters
    const char* getWiFiSSID() const { return config.wifi.ssid; }
    const char* getWiFiPassword() const { return config.wifi.password; }
    void setWiFiCredentials(const char* ssid, const char* password);
    
    // MQTT getters/setters
    bool isMQTTEnabled() const { return config.mqtt.enabled; }
    const char* getMQTTServer() const { return config.mqtt.server; }
    uint16_t getMQTTPort() const { return config.mqtt.port; }
    const char* getMQTTUsername() const { return config.mqtt.username; }
    const char* getMQTTPassword() const { return config.mqtt.password; }
    const char* getMQTTClientId() const { return config.mqtt.clientId; }
    const char* getMQTTBaseTopic() const { return config.mqtt.baseTopic; }
    void setMQTTEnabled(bool enabled);
    void setMQTTConfig(const char* server, uint16_t port, const char* username, 
                       const char* password, const char* clientId, const char* baseTopic);
    
    // Awning getters/setters
    unsigned long getTravelTime() const { return config.awning.travelTimeMs; }
    unsigned long getWindThreshold() const { return config.awning.windThreshold; }
    float getCurrentPosition() const { return config.awning.currentPosition; }
    float getTargetPosition() const { return config.awning.targetPosition; }
    void setTravelTime(unsigned long timeMs);
    void setWindThreshold(unsigned long threshold);
    void setCurrentPosition(float position);
    void setTargetPosition(float position);
    
    // Validation
    bool isConfigValid() const { return configValid; }
    bool hasWiFiConfig() const;
    bool hasMQTTConfig() const;
};

#endif // CONFIG_MANAGER_H