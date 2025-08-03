#ifndef CONSTANTS_H
#define CONSTANTS_H

// Timing Constants
const unsigned long MOTOR_START_PULSE_MS = 1000;
const unsigned long MOTOR_STOP_PULSE_MS = 100;
const unsigned long BUTTON_DEBOUNCE_MS = 20;
const unsigned long BUTTON_LONG_PRESS_MS = 1000;
const unsigned long WIND_SENSOR_DEBOUNCE_MS = 10;
const unsigned long POSITION_UPDATE_INTERVAL_MS = 100;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 1000;
const unsigned long MQTT_CONNECTION_TIMEOUT_MS = 10000;
const unsigned long MQTT_MAX_FAILED_ATTEMPTS = 5;
const unsigned long MQTT_BACKOFF_BASE_MS = 30000;
const unsigned long MOTOR_PULSE_DELAY_MS = 500;

// Position Constants
const float POSITION_TOLERANCE = 1.0;
const float MIN_POSITION = 0.0;
const float MAX_POSITION = 100.0;

// EEPROM Constants
const int EEPROM_SIZE = 512;
const uint32_t EEPROM_MAGIC_VALUE = 0xDEADBEEF;

// Default Settings
const unsigned long DEFAULT_TRAVEL_TIME_MS = 15000;  // 15 seconds default travel time
const unsigned long DEFAULT_WIND_PULSE_THRESHOLD = 100;  // 100 pulses per minute default threshold

// Limits
const unsigned long MIN_TRAVEL_TIME_MS = 5000;
const unsigned long MAX_TRAVEL_TIME_MS = 300000;
const unsigned long MIN_WIND_PULSE_THRESHOLD = 0;
const unsigned long MAX_WIND_PULSE_THRESHOLD = 1000;

#endif // CONSTANTS_H