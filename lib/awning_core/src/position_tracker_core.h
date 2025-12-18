#ifndef POSITION_TRACKER_CORE_H
#define POSITION_TRACKER_CORE_H

#include "awning_types.h"

// Platform-independent position tracking logic
class PositionTrackerCore {
private:
    float currentPosition;
    float targetPosition;
    unsigned long travelTimeMs;

public:
    PositionTrackerCore()
        : currentPosition(0.0f)
        , targetPosition(0.0f)
        , travelTimeMs(DEFAULT_TRAVEL_TIME_MS) {}

    void setTravelTime(unsigned long timeMs) {
        travelTimeMs = clamp(timeMs, MIN_TRAVEL_TIME_MS, MAX_TRAVEL_TIME_MS);
    }

    void setCurrentPosition(float position) {
        currentPosition = clamp(position, MIN_POSITION, MAX_POSITION);
    }

    void setTargetPosition(float position) {
        targetPosition = clamp(position, MIN_POSITION, MAX_POSITION);
    }

    float getCurrentPosition() const { return currentPosition; }
    float getTargetPosition() const { return targetPosition; }
    unsigned long getTravelTime() const { return travelTimeMs; }

    float calculatePositionChange(unsigned long deltaTimeMs) const {
        return static_cast<float>(deltaTimeMs) / static_cast<float>(travelTimeMs) * 100.0f;
    }

    bool hasReachedTarget() const {
        return std::abs(currentPosition - targetPosition) < POSITION_TOLERANCE;
    }

    bool hasReachedLimit(MotorDirection direction) const {
        if (direction == MOTOR_DIR_EXTENDING) {
            return currentPosition >= MAX_POSITION;
        }
        if (direction == MOTOR_DIR_RETRACTING) {
            return currentPosition <= MIN_POSITION;
        }
        return false;
    }

    void updatePosition(MotorDirection direction, unsigned long deltaTimeMs) {
        float change = calculatePositionChange(deltaTimeMs);
        if (direction == MOTOR_DIR_EXTENDING) {
            currentPosition = clamp(currentPosition + change, MIN_POSITION, MAX_POSITION);
        } else if (direction == MOTOR_DIR_RETRACTING) {
            currentPosition = clamp(currentPosition - change, MIN_POSITION, MAX_POSITION);
        }
    }

    MotorDirection getRequiredDirection() const {
        if (hasReachedTarget()) {
            return MOTOR_DIR_IDLE;
        }
        return (targetPosition > currentPosition) ? MOTOR_DIR_EXTENDING : MOTOR_DIR_RETRACTING;
    }
};

#endif // POSITION_TRACKER_CORE_H
