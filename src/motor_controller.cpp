#include "motor_controller.h"

MotorController::MotorController()
    : core(&relayHardware, &timeProvider) {
}

void MotorController::begin() {
    pinMode(RELAY_EXTEND, OUTPUT);
    pinMode(RELAY_RETRACT, OUTPUT);
    digitalWrite(RELAY_EXTEND, LOW);
    digitalWrite(RELAY_RETRACT, LOW);
}

void MotorController::update() {
    core.update(timeProvider.millis());
}

MotorState MotorController::getState() const {
    MotorOperationState opState = core.getOperationState();
    switch (opState) {
        case MOTOR_OP_EXTENDING:
            return MOTOR_EXTENDING;
        case MOTOR_RETRACTING:
            return MOTOR_RETRACTING;
        default:
            return MOTOR_IDLE;
    }
}

bool MotorController::isMoving() const {
    return core.isMoving();
}

bool MotorController::isBusy() const {
    return core.isBusy();
}

unsigned long MotorController::getRunTime() const {
    return core.getRunTime();
}

void MotorController::sendStartPulse(uint8_t relayPin) {
    core.requestStartPulse(relayPin);
    while (core.isBusy()) {
        update();
        yield();
    }
}

void MotorController::sendStopPulse(uint8_t relayPin) {
    core.requestStopPulse(relayPin);
    while (core.isBusy()) {
        update();
        yield();
    }
}

void MotorController::deactivateRelays() {
    core.deactivateRelays();
}

void MotorController::start(MotorState direction) {
    if (direction != MOTOR_EXTENDING && direction != MOTOR_RETRACTING) {
        return;
    }

    uint8_t relayPin = (direction == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;

    if (getState() != MOTOR_IDLE) {
        uint8_t currentRelay = (getState() == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;
        stop(currentRelay, true);
    }

    sendStartPulse(relayPin);
}

void MotorController::startWithoutStop(MotorState direction) {
    if (direction != MOTOR_EXTENDING && direction != MOTOR_RETRACTING) {
        return;
    }

    if (relayHardware.isAnyRelayActive()) {
        deactivateRelays();
        delay(100);
    }

    uint8_t relayPin = (direction == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;
    sendStartPulse(relayPin);
}

void MotorController::stop(uint8_t relayPin, bool sendStopPulseFlag) {
    if (sendStopPulseFlag) {
        sendStopPulse(relayPin);
    }

    if (getState() == MOTOR_IDLE) {
        return;
    }

    if (relayHardware.isAnyRelayActive()) {
        deactivateRelays();
        delay(100);
    }

    core.stopMotor();
}

void MotorController::stopBothRelays() {
    sendStopPulse(RELAY_EXTEND);
    delay(50);
    sendStopPulse(RELAY_RETRACT);

    if (getState() == MOTOR_IDLE) {
        return;
    }

    if (relayHardware.isAnyRelayActive()) {
        deactivateRelays();
        delay(100);
    }

    core.stopMotor();
}