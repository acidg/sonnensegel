#ifndef AWNING_CONTROLLER_H
#define AWNING_CONTROLLER_H

#include <Arduino.h>
#include "motor_controller.h"
#include "position_tracker.h"
#include "awning_state_machine.h"

// Arduino wrapper around AwningStateMachine
// Provides millis()-based timing
class AwningController {
private:
    AwningStateMachine stateMachine;
    PositionTracker& positionTracker;
    MotorController& motor;

public:
    AwningController(MotorController& motorController, PositionTracker& tracker);

    // Commands - delegate to state machine
    void setTarget(float target) { stateMachine.setTarget(target); }
    void stop(uint8_t relayPin) { stateMachine.stop(relayPin); }
    void stopBoth() { stateMachine.stopBoth(); }

    // Update loop - call every iteration
    void update();

    // State queries - delegate to state machine
    AwningState getState() const { return stateMachine.getState(); }
    float getTargetPosition() const { return stateMachine.getTargetPosition(); }
    float getCurrentPosition() const { return stateMachine.getCurrentPosition(); }
    bool isMoving() const { return stateMachine.isMoving(); }
    uint8_t getLastMovementRelay() const { return stateMachine.getLastMovementRelay(); }

    // For settings persistence
    void setCurrentPosition(float position) { stateMachine.setCurrentPosition(position); }

    // Access to position tracker for travel time settings
    PositionTracker& getPositionTracker() { return positionTracker; }
};

#endif // AWNING_CONTROLLER_H
