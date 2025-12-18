#ifndef AWNING_STATE_MACHINE_H
#define AWNING_STATE_MACHINE_H

#include "awning_types.h"
#include "position_tracker_core.h"

// Hardware abstraction interface - implement for each platform
class IMotorHardware {
public:
    virtual ~IMotorHardware() = default;
    virtual void sendStartPulse(uint8_t relayPin) = 0;
    virtual void sendStopPulse(uint8_t relayPin) = 0;
    virtual void deactivateRelays() = 0;
};

// Platform-independent awning state machine
class AwningStateMachine {
private:
    PositionTrackerCore& positionTracker;
    IMotorHardware* motorHardware;  // nullptr for pure logic testing
    AwningState state;
    float targetPosition;
    uint8_t lastMovementRelay;
    unsigned long lastUpdateTime;

    AwningState getDirectionForTarget(float target) const {
        float current = positionTracker.getCurrentPosition();
        if (target > current + POSITION_TOLERANCE) {
            return AWNING_EXTENDING;
        }
        if (target < current - POSITION_TOLERANCE) {
            return AWNING_RETRACTING;
        }
        return AWNING_IDLE;
    }

    bool hasReachedTarget() const {
        return std::abs(positionTracker.getCurrentPosition() - targetPosition) < POSITION_TOLERANCE;
    }

    void startMotor(AwningState direction) {
        if (direction == AWNING_EXTENDING) {
            lastMovementRelay = PIN_RELAY_EXTEND;
            if (motorHardware) {
                motorHardware->sendStartPulse(PIN_RELAY_EXTEND);
            }
        } else if (direction == AWNING_RETRACTING) {
            lastMovementRelay = PIN_RELAY_RETRACT;
            if (motorHardware) {
                motorHardware->sendStartPulse(PIN_RELAY_RETRACT);
            }
        }
        state = direction;
    }

    void stopMotor(uint8_t relayPin, bool sendPulse) {
        if (sendPulse && motorHardware) {
            motorHardware->sendStopPulse(relayPin);
        }
        if (motorHardware) {
            motorHardware->deactivateRelays();
        }
        state = AWNING_IDLE;
    }

public:
    AwningStateMachine(PositionTrackerCore& tracker, IMotorHardware* hardware = nullptr)
        : positionTracker(tracker)
        , motorHardware(hardware)
        , state(AWNING_IDLE)
        , targetPosition(0.0f)
        , lastMovementRelay(PIN_RELAY_EXTEND)
        , lastUpdateTime(0) {}

    void setTarget(float target) {
        target = clamp(target, MIN_POSITION, MAX_POSITION);
        targetPosition = target;

        AwningState requiredDirection = getDirectionForTarget(target);

        // Already at target
        if (requiredDirection == AWNING_IDLE) {
            if (isMoving()) {
                stopMotor(lastMovementRelay, true);
            }
            return;
        }

        // From IDLE: start moving
        if (state == AWNING_IDLE) {
            startMotor(requiredDirection);
            return;
        }

        // Already moving in correct direction
        if (state == requiredDirection) {
            return;
        }

        // Direction change needed
        if (motorHardware) {
            motorHardware->deactivateRelays();
        }
        startMotor(requiredDirection);
    }

    void stop(uint8_t relayPin) {
        targetPosition = positionTracker.getCurrentPosition();
        stopMotor(relayPin, true);
    }

    void stopBoth() {
        targetPosition = positionTracker.getCurrentPosition();
        if (motorHardware) {
            motorHardware->sendStopPulse(PIN_RELAY_EXTEND);
            motorHardware->sendStopPulse(PIN_RELAY_RETRACT);
            motorHardware->deactivateRelays();
        }
        state = AWNING_IDLE;
    }

    void update(unsigned long currentTimeMs) {
        // In IDLE: do nothing, wait for command
        if (state == AWNING_IDLE) {
            lastUpdateTime = currentTimeMs;
            return;
        }

        // Calculate time delta
        if (lastUpdateTime == 0) {
            lastUpdateTime = currentTimeMs;
            return;
        }

        unsigned long deltaTime = currentTimeMs - lastUpdateTime;
        if (deltaTime < POSITION_UPDATE_INTERVAL_MS) {
            return;
        }

        // Update position based on direction
        MotorDirection dir = (state == AWNING_EXTENDING) ? MOTOR_DIR_EXTENDING : MOTOR_DIR_RETRACTING;
        positionTracker.updatePosition(dir, deltaTime);
        lastUpdateTime = currentTimeMs;

        // Check if we've reached the target or limits
        float current = positionTracker.getCurrentPosition();
        bool atTarget = hasReachedTarget();
        bool atLimit = (state == AWNING_EXTENDING && current >= MAX_POSITION) ||
                       (state == AWNING_RETRACTING && current <= MIN_POSITION);

        if (atTarget || atLimit) {
            bool sendPulse = !atLimit;  // Don't send pulse at limits
            stopMotor(lastMovementRelay, sendPulse);
        }
    }

    // State queries
    AwningState getState() const { return state; }
    float getTargetPosition() const { return targetPosition; }
    float getCurrentPosition() const { return positionTracker.getCurrentPosition(); }
    bool isMoving() const { return state == AWNING_EXTENDING || state == AWNING_RETRACTING; }
    uint8_t getLastMovementRelay() const { return lastMovementRelay; }

    void setCurrentPosition(float position) {
        positionTracker.setCurrentPosition(position);
        targetPosition = position;
    }
};

#endif // AWNING_STATE_MACHINE_H
