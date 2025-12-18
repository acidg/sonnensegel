#include "motor_controller.h"

MotorController::MotorController()
    : state(MOTOR_IDLE), motorStartTime(0) {
}

void MotorController::begin() {
    pinMode(RELAY_EXTEND, OUTPUT);
    pinMode(RELAY_RETRACT, OUTPUT);
    digitalWrite(RELAY_EXTEND, LOW);
    digitalWrite(RELAY_RETRACT, LOW);
}

bool MotorController::isRelayActive() const {
    return digitalRead(RELAY_EXTEND) == HIGH || digitalRead(RELAY_RETRACT) == HIGH;
}

void MotorController::deactivateRelays() {
    digitalWrite(RELAY_EXTEND, LOW);
    digitalWrite(RELAY_RETRACT, LOW);
}

void MotorController::sendPulse(uint8_t relayPin, unsigned long duration) {
    if (isRelayActive()) {
        return;
    }

    digitalWrite(relayPin, HIGH);
    delay(duration);
    digitalWrite(relayPin, LOW);
}

// IMotorHardware interface implementation
void MotorController::sendStartPulse(uint8_t relayPin) {
    sendPulse(relayPin, MOTOR_START_PULSE_MS);
    state = (relayPin == RELAY_EXTEND) ? MOTOR_EXTENDING : MOTOR_RETRACTING;
    motorStartTime = millis();
}

void MotorController::sendStopPulse(uint8_t relayPin) {
    sendPulse(relayPin, MOTOR_STOP_PULSE_MS);
}

void MotorController::start(MotorState direction) {
    if (direction != MOTOR_EXTENDING && direction != MOTOR_RETRACTING) {
        return;
    }

    uint8_t relayPin = (direction == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;

    if (state != MOTOR_IDLE) {
        // Stop using the relay for the current direction
        uint8_t currentRelay = (state == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;
        stop(currentRelay, true);
        delay(MOTOR_PULSE_DELAY_MS);
    }

    sendPulse(relayPin, MOTOR_START_PULSE_MS);

    state = direction;
    motorStartTime = millis();
}

void MotorController::startWithoutStop(MotorState direction) {
    if (direction != MOTOR_EXTENDING && direction != MOTOR_RETRACTING) {
        return;
    }
    
    // Deactivate any active relays first (no stop pulse)
    if (isRelayActive()) {
        deactivateRelays();
        delay(100); // Brief pause to let relays settle
    }
    
    uint8_t relayPin = (direction == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;
    sendPulse(relayPin, MOTOR_START_PULSE_MS);
    
    state = direction;
    motorStartTime = millis();
}

void MotorController::stop(uint8_t relayPin, bool sendStopPulse) {
    if (sendStopPulse) {
        sendPulse(relayPin, MOTOR_STOP_PULSE_MS);
    }

    if (state == MOTOR_IDLE) {
        return;
    }

    state = MOTOR_STOPPING;

    if (isRelayActive()) {
        deactivateRelays();
        delay(100);
    }

    state = MOTOR_IDLE;
}

void MotorController::stopBothRelays() {
    // Send stop pulse on both relays for reliable stop
    sendPulse(RELAY_EXTEND, MOTOR_STOP_PULSE_MS);
    delay(50);
    sendPulse(RELAY_RETRACT, MOTOR_STOP_PULSE_MS);

    if (state == MOTOR_IDLE) {
        return;
    }

    state = MOTOR_STOPPING;

    if (isRelayActive()) {
        deactivateRelays();
        delay(100);
    }

    state = MOTOR_IDLE;
}

unsigned long MotorController::getRunTime() const {
    if (!isMoving()) {
        return 0;
    }
    return millis() - motorStartTime;
}