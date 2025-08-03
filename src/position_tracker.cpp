#include "position_tracker.h"

PositionTracker::PositionTracker() 
    : currentPosition(0.0), targetPosition(0.0), 
      travelTimeMs(DEFAULT_TRAVEL_TIME_MS), lastUpdateTime(0) {
}

void PositionTracker::setTravelTime(unsigned long timeMs) {
    travelTimeMs = constrain(timeMs, MIN_TRAVEL_TIME_MS, MAX_TRAVEL_TIME_MS);
}

void PositionTracker::setCurrentPosition(float position) {
    currentPosition = constrain(position, MIN_POSITION, MAX_POSITION);
}

void PositionTracker::setTargetPosition(float position) {
    targetPosition = constrain(position, MIN_POSITION, MAX_POSITION);
}

float PositionTracker::calculatePositionChange(unsigned long deltaTime) const {
    return (float)deltaTime / (float)travelTimeMs * 100.0;
}

bool PositionTracker::hasReachedTarget() const {
    return abs(currentPosition - targetPosition) < POSITION_TOLERANCE;
}

bool PositionTracker::hasReachedLimit(MotorState direction) const {
    if (direction == MOTOR_EXTENDING) {
        return currentPosition >= MAX_POSITION;
    }
    if (direction == MOTOR_RETRACTING) {
        return currentPosition <= MIN_POSITION;
    }
    return false;
}

void PositionTracker::update(MotorState motorState) {
    if (motorState != MOTOR_EXTENDING && motorState != MOTOR_RETRACTING) {
        lastUpdateTime = millis();
        return;
    }
    
    unsigned long now = millis();
    if (now - lastUpdateTime < POSITION_UPDATE_INTERVAL_MS) {
        return;
    }
    
    unsigned long deltaTime = now - lastUpdateTime;
    float change = calculatePositionChange(deltaTime);
    
    if (motorState == MOTOR_EXTENDING) {
        currentPosition = constrain(currentPosition + change, MIN_POSITION, MAX_POSITION);
    } else {
        currentPosition = constrain(currentPosition - change, MIN_POSITION, MAX_POSITION);
    }
    
    lastUpdateTime = now;
}

bool PositionTracker::shouldStop(MotorState motorState) const {
    if (hasReachedTarget()) {
        return true;
    }
    
    // Only stop at limits if target is exactly at the limit
    // This allows drift compensation when target extends beyond limits
    if (hasReachedLimit(motorState)) {
        if (motorState == MOTOR_EXTENDING && targetPosition >= MAX_POSITION) {
            return true;
        }
        if (motorState == MOTOR_RETRACTING && targetPosition <= MIN_POSITION) {
            return true;
        }
    }
    
    return false;
}

bool PositionTracker::isStoppingAtLimit(MotorState motorState) const {
    // Return true when target is exactly at limits (0% or 100%)
    // This prevents sending stop pulse for drift compensation
    return (targetPosition >= MAX_POSITION) || (targetPosition <= MIN_POSITION);
}

MotorState PositionTracker::getRequiredDirection() const {
    if (hasReachedTarget()) {
        return MOTOR_IDLE;
    }
    
    return (targetPosition > currentPosition) ? MOTOR_EXTENDING : MOTOR_RETRACTING;
}