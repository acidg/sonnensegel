#ifndef POSITION_TRACKER_H
#define POSITION_TRACKER_H

#include <Arduino.h>
#include "constants.h"
#include "motor_controller.h"

class PositionTracker {
private:
    float currentPosition;
    float targetPosition;
    unsigned long travelTimeMs;
    unsigned long lastUpdateTime;
    
    float calculatePositionChange(unsigned long deltaTime) const;
    bool hasReachedTarget() const;
    bool hasReachedLimit(MotorState direction) const;
    
public:
    PositionTracker();
    void setTravelTime(unsigned long timeMs);
    void setCurrentPosition(float position);
    void setTargetPosition(float position);
    void update(MotorState motorState);
    
    float getCurrentPosition() const { return currentPosition; }
    float getTargetPosition() const { return targetPosition; }
    unsigned long getTravelTime() const { return travelTimeMs; }
    bool shouldStop(MotorState motorState) const;
    MotorState getRequiredDirection() const;
};

#endif // POSITION_TRACKER_H