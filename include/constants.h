#ifndef CONSTANTS_H
#define CONSTANTS_H

// Timing Constants
const unsigned long MOTOR_START_PULSE_MS = 1000;
const unsigned long MOTOR_STOP_PULSE_MS = 100;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long BUTTON_LONG_PRESS_MS = 1000;
const unsigned long POSITION_UPDATE_INTERVAL_MS = 100;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long WIND_MEASUREMENT_INTERVAL_MS = 1000;
const unsigned long WIND_SAFETY_CHECK_INTERVAL_MS = 5000;
const unsigned long MQTT_PUBLISH_INTERVAL_MS = 1000;
const unsigned long MOTOR_PULSE_DELAY_MS = 500;

// Position Constants
const float POSITION_TOLERANCE = 1.0;
const float MIN_POSITION = 0.0;
const float MAX_POSITION = 100.0;

// EEPROM Constants
const int EEPROM_SIZE = 512;
const uint32_t EEPROM_MAGIC_VALUE = 0xDEADBEEF;

// Limits
const unsigned long MIN_TRAVEL_TIME_MS = 5000;
const unsigned long MAX_TRAVEL_TIME_MS = 300000;
const float MIN_WIND_THRESHOLD = 0.0;
const float MAX_WIND_THRESHOLD = 100.0;
const float MIN_WIND_FACTOR = 0.1;
const float MAX_WIND_FACTOR = 10.0;

#endif // CONSTANTS_H