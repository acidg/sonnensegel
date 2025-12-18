#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "pins.h"
#include "constants.h"
#include "awning_state_machine.h"

enum MotorState {
    MOTOR_IDLE,
    MOTOR_EXTENDING,
    MOTOR_RETRACTING,
    MOTOR_STOPPING
};

class MotorController : public IMotorHardware {
private:
    MotorState state;
    unsigned long motorStartTime;

    bool isRelayActive() const;
    void sendPulse(uint8_t relayPin, unsigned long duration);

public:
    MotorController();
    void begin();
    void start(MotorState direction);
    void startWithoutStop(MotorState direction);
    void stop(uint8_t relayPin, bool sendStopPulse = true);
    void stopBothRelays();
    MotorState getState() const { return state; }
    unsigned long getRunTime() const;
    bool isMoving() const { return state == MOTOR_EXTENDING || state == MOTOR_RETRACTING; }

    // IMotorHardware interface implementation
    void sendStartPulse(uint8_t relayPin) override;
    void sendStopPulse(uint8_t relayPin) override;
    void deactivateRelays() override;
};

#endif // MOTOR_CONTROLLER_H