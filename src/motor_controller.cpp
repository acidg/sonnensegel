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

void MotorController::start(MotorState direction) {
    if (direction != MOTOR_EXTENDING && direction != MOTOR_RETRACTING) {
        return;
    }
    
    if (state != MOTOR_IDLE) {
        stop();
        delay(MOTOR_PULSE_DELAY_MS);
    }
    
    uint8_t relayPin = (direction == MOTOR_EXTENDING) ? RELAY_EXTEND : RELAY_RETRACT;
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

void MotorController::stop(bool sendStopPulse) {
    // Always send stop pulse when requested, even if already idle
    if (sendStopPulse) {
        sendPulse(RELAY_EXTEND, MOTOR_STOP_PULSE_MS);
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

unsigned long MotorController::getRunTime() const {
    if (!isMoving()) {
        return 0;
    }
    return millis() - motorStartTime;
}