#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "pins.h"
#include "constants.h"
#include "awning_state_machine.h"
#include "motor_controller_core.h"
#include "time_provider.h"

enum MotorState {
    MOTOR_IDLE,
    MOTOR_EXTENDING,
    MOTOR_RETRACTING,
    MOTOR_STOPPING
};

// Arduino time provider implementation
class ArduinoTimeProvider : public ITimeProvider {
public:
    unsigned long millis() const override {
        return ::millis();
    }
};

// Arduino relay hardware implementation
class ArduinoRelayHardware : public IRelayHardware {
public:
    void setRelayHigh(uint8_t relayPin) override {
        digitalWrite(relayPin, HIGH);
    }

    void setRelayLow(uint8_t relayPin) override {
        digitalWrite(relayPin, LOW);
    }

    void deactivateAllRelays() override {
        digitalWrite(RELAY_EXTEND, LOW);
        digitalWrite(RELAY_RETRACT, LOW);
    }

    bool isAnyRelayActive() const override {
        return digitalRead(RELAY_EXTEND) == HIGH ||
               digitalRead(RELAY_RETRACT) == HIGH;
    }
};

class MotorController : public IMotorHardware {
private:
    ArduinoRelayHardware relayHardware;
    ArduinoTimeProvider timeProvider;
    MotorControllerCore core;

public:
    MotorController();
    void begin();
    void update();
    void start(MotorState direction);
    void startWithoutStop(MotorState direction);
    void stop(uint8_t relayPin, bool sendStopPulse = true);
    void stopBothRelays();
    MotorState getState() const;
    unsigned long getRunTime() const;
    bool isMoving() const;
    bool isBusy() const;

    // IMotorHardware interface implementation
    void sendStartPulse(uint8_t relayPin) override;
    void sendStopPulse(uint8_t relayPin) override;
    void deactivateRelays() override;

    // Access to core for testing
    MotorControllerCore& getCore() { return core; }
};

#endif // MOTOR_CONTROLLER_H