#ifndef MOTOR_CONTROLLER_CORE_H
#define MOTOR_CONTROLLER_CORE_H

#include "awning_types.h"
#include "time_provider.h"

// Motor pulse states
enum MotorPulseState {
    MOTOR_PULSE_IDLE,
    MOTOR_PULSE_START_ACTIVE,
    MOTOR_PULSE_STOP_ACTIVE,
    MOTOR_PULSE_RELAY_SETTLING
};

// Motor operational states
enum MotorOperationState {
    MOTOR_OP_IDLE,
    MOTOR_OP_EXTENDING,
    MOTOR_OP_RETRACTING
};

// Hardware abstraction for relay control
class IRelayHardware {
public:
    virtual ~IRelayHardware() = default;
    virtual void setRelayHigh(uint8_t relayPin) = 0;
    virtual void setRelayLow(uint8_t relayPin) = 0;
    virtual void deactivateAllRelays() = 0;
    virtual bool isAnyRelayActive() const = 0;
};

// Platform-independent motor controller state machine
class MotorControllerCore {
private:
    IRelayHardware* relayHardware;
    ITimeProvider* timeProvider;

    MotorPulseState pulseState;
    MotorOperationState operationState;
    unsigned long pulseStartTime;
    unsigned long pulseDuration;
    uint8_t activePulseRelay;
    uint8_t lastMovementRelay;
    unsigned long motorStartTime;

    static constexpr unsigned long RELAY_SETTLING_TIME_MS = 100;

    void startPulse(uint8_t relayPin, unsigned long duration) {
        if (!relayHardware || pulseState != MOTOR_PULSE_IDLE) {
            return;
        }

        relayHardware->setRelayHigh(relayPin);
        activePulseRelay = relayPin;
        pulseDuration = duration;
        pulseStartTime = timeProvider ? timeProvider->millis() : 0;
        pulseState = (duration == MOTOR_START_PULSE_MS) ?
                     MOTOR_PULSE_START_ACTIVE : MOTOR_PULSE_STOP_ACTIVE;
    }

    void endPulse() {
        if (!relayHardware) {
            return;
        }

        relayHardware->setRelayLow(activePulseRelay);
        pulseStartTime = timeProvider ? timeProvider->millis() : 0;
        pulseState = MOTOR_PULSE_RELAY_SETTLING;
    }

    void finishSettling() {
        pulseState = MOTOR_PULSE_IDLE;
        activePulseRelay = 0;
    }

public:
    MotorControllerCore(IRelayHardware* hardware = nullptr,
                       ITimeProvider* timeProviderInstance = nullptr)
        : relayHardware(hardware)
        , timeProvider(timeProviderInstance)
        , pulseState(MOTOR_PULSE_IDLE)
        , operationState(MOTOR_OP_IDLE)
        , pulseStartTime(0)
        , pulseDuration(0)
        , activePulseRelay(0)
        , lastMovementRelay(PIN_RELAY_EXTEND)
        , motorStartTime(0) {
    }

    void update(unsigned long currentTimeMs) {
        if (!timeProvider) {
            return;
        }

        if (pulseState == MOTOR_PULSE_IDLE) {
            return;
        }

        unsigned long elapsed = currentTimeMs - pulseStartTime;

        if (pulseState == MOTOR_PULSE_START_ACTIVE ||
            pulseState == MOTOR_PULSE_STOP_ACTIVE) {
            if (elapsed >= pulseDuration) {
                endPulse();
            }
        }
        else if (pulseState == MOTOR_PULSE_RELAY_SETTLING) {
            if (elapsed >= RELAY_SETTLING_TIME_MS) {
                finishSettling();
            }
        }
    }

    void requestStartPulse(uint8_t relayPin) {
        if (relayHardware && relayHardware->isAnyRelayActive()) {
            return;
        }

        startPulse(relayPin, MOTOR_START_PULSE_MS);
        lastMovementRelay = relayPin;

        if (relayPin == PIN_RELAY_EXTEND) {
            operationState = MOTOR_OP_EXTENDING;
            motorStartTime = timeProvider ? timeProvider->millis() : 0;
        } else if (relayPin == PIN_RELAY_RETRACT) {
            operationState = MOTOR_OP_RETRACTING;
            motorStartTime = timeProvider ? timeProvider->millis() : 0;
        }
    }

    void requestStopPulse(uint8_t relayPin) {
        if (relayHardware && relayHardware->isAnyRelayActive()) {
            return;
        }

        startPulse(relayPin, MOTOR_STOP_PULSE_MS);
    }

    void deactivateRelays() {
        if (relayHardware) {
            relayHardware->deactivateAllRelays();
        }
        operationState = MOTOR_OP_IDLE;
    }

    void stopMotor() {
        operationState = MOTOR_OP_IDLE;
    }

    bool isBusy() const {
        return pulseState != MOTOR_PULSE_IDLE;
    }

    bool isMoving() const {
        return operationState == MOTOR_OP_EXTENDING ||
               operationState == MOTOR_OP_RETRACTING;
    }

    MotorOperationState getOperationState() const {
        return operationState;
    }

    MotorPulseState getPulseState() const {
        return pulseState;
    }

    uint8_t getLastMovementRelay() const {
        return lastMovementRelay;
    }

    unsigned long getRunTime() const {
        if (!isMoving() || !timeProvider) {
            return 0;
        }
        return timeProvider->millis() - motorStartTime;
    }
};

#endif // MOTOR_CONTROLLER_CORE_H
