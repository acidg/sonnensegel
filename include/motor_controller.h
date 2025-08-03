#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "pins.h"
#include "constants.h"

enum MotorState {
    MOTOR_IDLE,
    MOTOR_EXTENDING,
    MOTOR_RETRACTING,
    MOTOR_STOPPING
};

class MotorController {
private:
    MotorState state;
    unsigned long motorStartTime;
    
    bool isRelayActive() const;
    void sendPulse(uint8_t relayPin, unsigned long duration);
    void deactivateRelays();
    
public:
    MotorController();
    void begin();
    void start(MotorState direction);
    void startWithoutStop(MotorState direction);
    void stop(bool sendStopPulse = true);
    MotorState getState() const { return state; }
    unsigned long getRunTime() const;
    bool isMoving() const { return state == MOTOR_EXTENDING || state == MOTOR_RETRACTING; }
};

#endif // MOTOR_CONTROLLER_H