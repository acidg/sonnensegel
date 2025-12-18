#ifndef POSITION_TRACKER_H
#define POSITION_TRACKER_H

#include <Arduino.h>
#include "position_tracker_core.h"
#include "motor_controller.h"

// Arduino wrapper around PositionTrackerCore
// Adds millis()-based timing
class PositionTracker {
private:
    PositionTrackerCore core;
    unsigned long lastUpdateTime;

public:
    PositionTracker();
    void setTravelTime(unsigned long timeMs) { core.setTravelTime(timeMs); }
    void setCurrentPosition(float position) { core.setCurrentPosition(position); }
    void setTargetPosition(float position) { core.setTargetPosition(position); }
    void update(MotorState motorState);

    float getCurrentPosition() const { return core.getCurrentPosition(); }
    float getTargetPosition() const { return core.getTargetPosition(); }
    unsigned long getTravelTime() const { return core.getTravelTime(); }

    // Access to core for state machine integration
    PositionTrackerCore& getCore() { return core; }
};

#endif // POSITION_TRACKER_H