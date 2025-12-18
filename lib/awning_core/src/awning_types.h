#ifndef AWNING_TYPES_H
#define AWNING_TYPES_H

#include <cstdint>
#include <cmath>

// When used in unit tests, define constants here
// When used in application, constants come from constants.h and pins.h
#ifdef UNIT_TEST

// Position constants
constexpr float POSITION_TOLERANCE = 1.0f;
constexpr float MIN_POSITION = 0.0f;
constexpr float MAX_POSITION = 100.0f;

// Travel time limits
constexpr unsigned long MIN_TRAVEL_TIME_MS = 5000;
constexpr unsigned long MAX_TRAVEL_TIME_MS = 300000;
constexpr unsigned long DEFAULT_TRAVEL_TIME_MS = 15000;

// Motor timing
constexpr unsigned long MOTOR_START_PULSE_MS = 1000;
constexpr unsigned long MOTOR_STOP_PULSE_MS = 250;
constexpr unsigned long MOTOR_PULSE_DELAY_MS = 500;
constexpr unsigned long POSITION_UPDATE_INTERVAL_MS = 100;

// Pin definitions for tests
constexpr uint8_t PIN_RELAY_EXTEND = 14;
constexpr uint8_t PIN_RELAY_RETRACT = 12;

#else

// Use application constants
#include "constants.h"
#include "pins.h"

// Map pin names
constexpr uint8_t PIN_RELAY_EXTEND = RELAY_EXTEND;
constexpr uint8_t PIN_RELAY_RETRACT = RELAY_RETRACT;

#endif // UNIT_TEST

// State enums
enum MotorDirection {
    MOTOR_DIR_IDLE,
    MOTOR_DIR_EXTENDING,
    MOTOR_DIR_RETRACTING,
    MOTOR_DIR_STOPPING
};

enum AwningState {
    AWNING_IDLE,
    AWNING_EXTENDING,
    AWNING_RETRACTING
};

// Utility function
template<typename T>
inline T clamp(T value, T minVal, T maxVal) {
    if (value < minVal) { return minVal; }
    if (value > maxVal) { return maxVal; }
    return value;
}

#endif // AWNING_TYPES_H
