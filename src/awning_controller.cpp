#include "awning_controller.h"

AwningController::AwningController(MotorController& motorController, PositionTracker& tracker)
    : stateMachine(tracker.getCore(), &motorController)
    , positionTracker(tracker)
    , motor(motorController) {
}

void AwningController::update() {
    // Update position tracking based on current state
    MotorState motorState = MOTOR_IDLE;
    AwningState state = stateMachine.getState();
    if (state == AWNING_EXTENDING) {
        motorState = MOTOR_EXTENDING;
    } else if (state == AWNING_RETRACTING) {
        motorState = MOTOR_RETRACTING;
    }
    positionTracker.update(motorState);

    // Update state machine with current time
    stateMachine.update(millis());
}
