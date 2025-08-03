#include "config_manager.h"
#include "constants.h"
#include <EEPROM.h>
#include <string.h>

const uint32_t CONFIG_MAGIC = 0xABC12301;
const int CONFIG_EEPROM_ADDR = 0;

ConfigManager::ConfigManager() : configValid(false) {
    setDefaults();
}

bool ConfigManager::begin() {
    EEPROM.begin(EEPROM_SIZE);
    return load();
}

void ConfigManager::setDefaults() {
    config.magic = CONFIG_MAGIC;
    
    // WiFi defaults - empty for initial setup
    memset(config.wifi.ssid, 0, sizeof(config.wifi.ssid));
    memset(config.wifi.password, 0, sizeof(config.wifi.password));
    strncpy(config.wifi.hostname, "sonnensegel", sizeof(config.wifi.hostname) - 1);
    
    // MQTT defaults - disabled by default
    config.mqtt.enabled = false;
    strncpy(config.mqtt.server, "192.168.1.100", sizeof(config.mqtt.server) - 1);
    config.mqtt.port = 1883;
    memset(config.mqtt.username, 0, sizeof(config.mqtt.username));
    memset(config.mqtt.password, 0, sizeof(config.mqtt.password));
    strncpy(config.mqtt.clientId, "sonnensegel", sizeof(config.mqtt.clientId) - 1);
    strncpy(config.mqtt.baseTopic, "home/sonnensegel", sizeof(config.mqtt.baseTopic) - 1);
    
    // Awning defaults
    config.awning.travelTimeMs = DEFAULT_TRAVEL_TIME_MS;
    config.awning.windThreshold = DEFAULT_WIND_PULSE_THRESHOLD;
    config.awning.currentPosition = 0.0;
    config.awning.targetPosition = 0.0;
    
    config.checksum = calculateChecksum(config);
}

uint32_t ConfigManager::calculateChecksum(const SystemConfig& cfg) const {
    uint32_t checksum = 0;
    const uint8_t* data = (const uint8_t*)&cfg;
    size_t size = sizeof(SystemConfig) - sizeof(uint32_t); // Exclude checksum field
    
    for (size_t i = 0; i < size; i++) {
        checksum += data[i];
    }
    return checksum;
}

bool ConfigManager::load() {
    EEPROM.get(CONFIG_EEPROM_ADDR, config);
    
    if (config.magic != CONFIG_MAGIC) {
        Serial.println("Config: Invalid magic, using defaults");
        setDefaults();
        configValid = false;
        return false;
    }
    
    uint32_t expectedChecksum = calculateChecksum(config);
    if (config.checksum != expectedChecksum) {
        Serial.println("Config: Checksum mismatch, using defaults");
        setDefaults();
        configValid = false;
        return false;
    }
    
    configValid = true;
    Serial.println("Config: Loaded successfully");
    return true;
}

bool ConfigManager::save() {
    config.checksum = calculateChecksum(config);
    EEPROM.put(CONFIG_EEPROM_ADDR, config);
    bool success = EEPROM.commit();
    
    if (success) {
        configValid = true;
        Serial.println("Config: Saved successfully");
    } else {
        Serial.println("Config: Save failed");
    }
    
    return success;
}

void ConfigManager::reset() {
    setDefaults();
    save();
}

void ConfigManager::setWiFiCredentials(const char* ssid, const char* password) {
    strncpy(config.wifi.ssid, ssid, sizeof(config.wifi.ssid) - 1);
    config.wifi.ssid[sizeof(config.wifi.ssid) - 1] = '\0';
    
    strncpy(config.wifi.password, password, sizeof(config.wifi.password) - 1);
    config.wifi.password[sizeof(config.wifi.password) - 1] = '\0';
}

void ConfigManager::setHostname(const char* hostname) {
    strncpy(config.wifi.hostname, hostname, sizeof(config.wifi.hostname) - 1);
    config.wifi.hostname[sizeof(config.wifi.hostname) - 1] = '\0';
}

void ConfigManager::setMQTTEnabled(bool enabled) {
    config.mqtt.enabled = enabled;
}

void ConfigManager::setMQTTConfig(const char* server, uint16_t port, const char* username, 
                                  const char* password, const char* clientId, const char* baseTopic) {
    strncpy(config.mqtt.server, server, sizeof(config.mqtt.server) - 1);
    config.mqtt.server[sizeof(config.mqtt.server) - 1] = '\0';
    
    config.mqtt.port = port;
    
    strncpy(config.mqtt.username, username, sizeof(config.mqtt.username) - 1);
    config.mqtt.username[sizeof(config.mqtt.username) - 1] = '\0';
    
    strncpy(config.mqtt.password, password, sizeof(config.mqtt.password) - 1);
    config.mqtt.password[sizeof(config.mqtt.password) - 1] = '\0';
    
    strncpy(config.mqtt.clientId, clientId, sizeof(config.mqtt.clientId) - 1);
    config.mqtt.clientId[sizeof(config.mqtt.clientId) - 1] = '\0';
    
    strncpy(config.mqtt.baseTopic, baseTopic, sizeof(config.mqtt.baseTopic) - 1);
    config.mqtt.baseTopic[sizeof(config.mqtt.baseTopic) - 1] = '\0';
}

void ConfigManager::setTravelTime(unsigned long timeMs) {
    config.awning.travelTimeMs = constrain(timeMs, MIN_TRAVEL_TIME_MS, MAX_TRAVEL_TIME_MS);
}

void ConfigManager::setWindThreshold(unsigned long threshold) {
    config.awning.windThreshold = constrain(threshold, MIN_WIND_PULSE_THRESHOLD, MAX_WIND_PULSE_THRESHOLD);
}

void ConfigManager::setCurrentPosition(float position) {
    config.awning.currentPosition = constrain(position, MIN_POSITION, MAX_POSITION);
}

void ConfigManager::setTargetPosition(float position) {
    config.awning.targetPosition = constrain(position, MIN_POSITION, MAX_POSITION);
}

bool ConfigManager::hasWiFiConfig() const {
    return strlen(config.wifi.ssid) > 0;
}

bool ConfigManager::hasMQTTConfig() const {
    return strlen(config.mqtt.server) > 0;
}