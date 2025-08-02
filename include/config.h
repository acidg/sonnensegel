#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// MQTT Configuration
#define MQTT_SERVER "your_mqtt_server"
#define MQTT_PORT 1883
#define MQTT_USER "your_mqtt_username"
#define MQTT_PASSWORD "your_mqtt_password"
#define MQTT_CLIENT_ID "awning_controller"
#define MQTT_BASE_TOPIC "home/awning"

// Default Settings
#define DEFAULT_TRAVEL_TIME_MS 15000  // 15 seconds default travel time
#define DEFAULT_WIND_THRESHOLD 25.0   // 25 km/h default wind threshold
#define DEFAULT_WIND_FACTOR 2.5       // Default conversion factor (pulses/sec to km/h)

#endif // CONFIG_H