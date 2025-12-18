#include "position_tracker.h"

PositionTracker::PositionTracker()
    : lastUpdateTime(0) {
}

void PositionTracker::update(MotorState motorState) {
    unsigned long now = millis();

    if (motorState != MOTOR_EXTENDING && motorState != MOTOR_RETRACTING) {
        lastUpdateTime = now;
        return;
    }

    if (now - lastUpdateTime < POSITION_UPDATE_INTERVAL_MS) {
        return;
    }

    unsigned long deltaTime = now - lastUpdateTime;
    MotorDirection dir = (motorState == MOTOR_EXTENDING) ? MOTOR_DIR_EXTENDING : MOTOR_DIR_RETRACTING;
    core.updatePosition(dir, deltaTime);

    lastUpdateTime = now;
}
