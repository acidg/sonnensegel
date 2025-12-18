#include <unity.h>
#include "motor_controller_core.h"

// Mock implementations for testing
class MockTimeProvider : public ITimeProvider {
private:
    unsigned long currentTime;

public:
    MockTimeProvider() : currentTime(0) {}

    unsigned long millis() const override {
        return currentTime;
    }

    void setTime(unsigned long time) {
        currentTime = time;
    }

    void advance(unsigned long ms) {
        currentTime += ms;
    }
};

class MockRelayHardware : public IRelayHardware {
private:
    bool relayExtendHigh;
    bool relayRetractHigh;

public:
    MockRelayHardware() : relayExtendHigh(false), relayRetractHigh(false) {}

    void setRelayHigh(uint8_t relayPin) override {
        if (relayPin == PIN_RELAY_EXTEND) {
            relayExtendHigh = true;
        } else if (relayPin == PIN_RELAY_RETRACT) {
            relayRetractHigh = true;
        }
    }

    void setRelayLow(uint8_t relayPin) override {
        if (relayPin == PIN_RELAY_EXTEND) {
            relayExtendHigh = false;
        } else if (relayPin == PIN_RELAY_RETRACT) {
            relayRetractHigh = false;
        }
    }

    void deactivateAllRelays() override {
        relayExtendHigh = false;
        relayRetractHigh = false;
    }

    bool isAnyRelayActive() const override {
        return relayExtendHigh || relayRetractHigh;
    }

    bool isRelayHigh(uint8_t relayPin) const {
        if (relayPin == PIN_RELAY_EXTEND) {
            return relayExtendHigh;
        }
        if (relayPin == PIN_RELAY_RETRACT) {
            return relayRetractHigh;
        }
        return false;
    }
};

// Test fixtures
static MockTimeProvider* timeProvider;
static MockRelayHardware* relayHardware;
static MotorControllerCore* motor;

void setUp() {
    timeProvider = new MockTimeProvider();
    relayHardware = new MockRelayHardware();
    motor = new MotorControllerCore(relayHardware, timeProvider);
}

void tearDown() {
    delete motor;
    delete relayHardware;
    delete timeProvider;
}

// =============================================================================
// Initial State Tests
// =============================================================================

void test_initial_state_is_idle() {
    TEST_ASSERT_EQUAL(MOTOR_OP_IDLE, motor->getOperationState());
    TEST_ASSERT_EQUAL(MOTOR_PULSE_IDLE, motor->getPulseState());
}

void test_initial_not_busy() {
    TEST_ASSERT_FALSE(motor->isBusy());
}

void test_initial_not_moving() {
    TEST_ASSERT_FALSE(motor->isMoving());
}

// =============================================================================
// Start Pulse Tests
// =============================================================================

void test_request_start_pulse_activates_relay() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_TRUE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));
    TEST_ASSERT_EQUAL(MOTOR_PULSE_START_ACTIVE, motor->getPulseState());
}

void test_start_pulse_sets_operation_state() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_EQUAL(MOTOR_OP_EXTENDING, motor->getOperationState());
    TEST_ASSERT_TRUE(motor->isMoving());
}

void test_start_pulse_makes_controller_busy() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_TRUE(motor->isBusy());
}

void test_start_pulse_completes_after_duration() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->setTime(0);

    motor->update(timeProvider->millis());
    TEST_ASSERT_TRUE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));

    // Advance time to pulse duration
    timeProvider->advance(MOTOR_START_PULSE_MS);
    motor->update(timeProvider->millis());

    TEST_ASSERT_FALSE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));
    TEST_ASSERT_EQUAL(MOTOR_PULSE_RELAY_SETTLING, motor->getPulseState());
}

void test_pulse_settling_completes() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->setTime(0);

    // Complete pulse
    timeProvider->advance(MOTOR_START_PULSE_MS);
    motor->update(timeProvider->millis());
    TEST_ASSERT_EQUAL(MOTOR_PULSE_RELAY_SETTLING, motor->getPulseState());

    // Complete settling
    timeProvider->advance(100);
    motor->update(timeProvider->millis());

    TEST_ASSERT_EQUAL(MOTOR_PULSE_IDLE, motor->getPulseState());
    TEST_ASSERT_FALSE(motor->isBusy());
}

void test_motor_remains_extending_after_pulse_completes() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->setTime(0);

    // Complete pulse
    timeProvider->advance(MOTOR_START_PULSE_MS);
    motor->update(timeProvider->millis());

    // Complete settling
    timeProvider->advance(100);
    motor->update(timeProvider->millis());

    TEST_ASSERT_EQUAL(MOTOR_OP_EXTENDING, motor->getOperationState());
    TEST_ASSERT_TRUE(motor->isMoving());
    TEST_ASSERT_FALSE(motor->isBusy());
}

// =============================================================================
// Stop Pulse Tests
// =============================================================================

void test_request_stop_pulse_activates_relay() {
    motor->requestStopPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_TRUE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));
    TEST_ASSERT_EQUAL(MOTOR_PULSE_STOP_ACTIVE, motor->getPulseState());
}

void test_stop_pulse_completes_after_duration() {
    motor->requestStopPulse(PIN_RELAY_EXTEND);
    timeProvider->setTime(0);

    motor->update(timeProvider->millis());
    TEST_ASSERT_TRUE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));

    // Advance time to stop pulse duration
    timeProvider->advance(MOTOR_STOP_PULSE_MS);
    motor->update(timeProvider->millis());

    TEST_ASSERT_FALSE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));
    TEST_ASSERT_EQUAL(MOTOR_PULSE_RELAY_SETTLING, motor->getPulseState());
}

// =============================================================================
// Direction Tests
// =============================================================================

void test_start_pulse_extend_sets_extending() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_EQUAL(MOTOR_OP_EXTENDING, motor->getOperationState());
    TEST_ASSERT_EQUAL(PIN_RELAY_EXTEND, motor->getLastMovementRelay());
}

void test_start_pulse_retract_sets_retracting() {
    motor->requestStartPulse(PIN_RELAY_RETRACT);

    TEST_ASSERT_EQUAL(MOTOR_OP_RETRACTING, motor->getOperationState());
    TEST_ASSERT_EQUAL(PIN_RELAY_RETRACT, motor->getLastMovementRelay());
}

// =============================================================================
// Deactivate Tests
// =============================================================================

void test_deactivate_stops_motor() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->advance(MOTOR_START_PULSE_MS + 100);
    motor->update(timeProvider->millis());

    TEST_ASSERT_TRUE(motor->isMoving());

    motor->deactivateRelays();

    TEST_ASSERT_FALSE(motor->isMoving());
    TEST_ASSERT_EQUAL(MOTOR_OP_IDLE, motor->getOperationState());
}

void test_deactivate_clears_all_relays() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    motor->deactivateRelays();

    TEST_ASSERT_FALSE(relayHardware->isAnyRelayActive());
}

// =============================================================================
// Busy State Tests
// =============================================================================

void test_busy_during_start_pulse() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_TRUE(motor->isBusy());
}

void test_busy_during_stop_pulse() {
    motor->requestStopPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_TRUE(motor->isBusy());
}

void test_busy_during_settling() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->setTime(0);
    timeProvider->advance(MOTOR_START_PULSE_MS);
    motor->update(timeProvider->millis());

    TEST_ASSERT_EQUAL(MOTOR_PULSE_RELAY_SETTLING, motor->getPulseState());
    TEST_ASSERT_TRUE(motor->isBusy());
}

void test_not_busy_after_complete() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->setTime(0);

    // Complete pulse
    timeProvider->advance(MOTOR_START_PULSE_MS);
    motor->update(timeProvider->millis());

    // Complete settling
    timeProvider->advance(100);
    motor->update(timeProvider->millis());

    TEST_ASSERT_FALSE(motor->isBusy());
}

// =============================================================================
// Interlock Tests
// =============================================================================

void test_cannot_send_pulse_while_busy() {
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    TEST_ASSERT_TRUE(motor->isBusy());

    // Try to send another pulse
    motor->requestStopPulse(PIN_RELAY_RETRACT);

    // Should still be in start pulse state
    TEST_ASSERT_EQUAL(MOTOR_PULSE_START_ACTIVE, motor->getPulseState());
    TEST_ASSERT_TRUE(relayHardware->isRelayHigh(PIN_RELAY_EXTEND));
    TEST_ASSERT_FALSE(relayHardware->isRelayHigh(PIN_RELAY_RETRACT));
}

void test_cannot_send_pulse_when_relay_active() {
    relayHardware->setRelayHigh(PIN_RELAY_EXTEND);

    motor->requestStartPulse(PIN_RELAY_RETRACT);

    TEST_ASSERT_EQUAL(MOTOR_PULSE_IDLE, motor->getPulseState());
}

// =============================================================================
// Runtime Tests
// =============================================================================

void test_runtime_zero_when_idle() {
    TEST_ASSERT_EQUAL(0, motor->getRunTime());
}

void test_runtime_tracks_extending() {
    timeProvider->setTime(1000);
    motor->requestStartPulse(PIN_RELAY_EXTEND);

    // Complete start pulse
    timeProvider->advance(MOTOR_START_PULSE_MS);
    motor->update(timeProvider->millis());

    // Complete settling
    timeProvider->advance(100);
    motor->update(timeProvider->millis());

    // Now motor is running, advance 5 more seconds
    timeProvider->advance(5000);
    unsigned long runtime = motor->getRunTime();

    // Runtime should be from start of pulse (1000ms) to now (1000 + 1000 + 100 + 5000 = 7100ms)
    // Which is 6100ms elapsed
    TEST_ASSERT_UINT_WITHIN(100, 6100, runtime);
}

void test_runtime_zero_after_stop() {
    timeProvider->setTime(1000);
    motor->requestStartPulse(PIN_RELAY_EXTEND);
    timeProvider->advance(5000);

    motor->stopMotor();

    TEST_ASSERT_EQUAL(0, motor->getRunTime());
}

// =============================================================================
// Update Without TimeProvider Tests
// =============================================================================

void test_update_without_time_provider_safe() {
    delete motor;
    motor = new MotorControllerCore(relayHardware, nullptr);

    motor->update(1000);

    // Should not crash
    TEST_ASSERT_EQUAL(MOTOR_PULSE_IDLE, motor->getPulseState());
}

// =============================================================================
// Test Runner
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initial state
    RUN_TEST(test_initial_state_is_idle);
    RUN_TEST(test_initial_not_busy);
    RUN_TEST(test_initial_not_moving);

    // Start pulse behavior
    RUN_TEST(test_request_start_pulse_activates_relay);
    RUN_TEST(test_start_pulse_sets_operation_state);
    RUN_TEST(test_start_pulse_makes_controller_busy);
    RUN_TEST(test_start_pulse_completes_after_duration);
    RUN_TEST(test_pulse_settling_completes);
    RUN_TEST(test_motor_remains_extending_after_pulse_completes);

    // Stop pulse behavior
    RUN_TEST(test_request_stop_pulse_activates_relay);
    RUN_TEST(test_stop_pulse_completes_after_duration);

    // Direction
    RUN_TEST(test_start_pulse_extend_sets_extending);
    RUN_TEST(test_start_pulse_retract_sets_retracting);

    // Deactivate
    RUN_TEST(test_deactivate_stops_motor);
    RUN_TEST(test_deactivate_clears_all_relays);

    // Busy state
    RUN_TEST(test_busy_during_start_pulse);
    RUN_TEST(test_busy_during_stop_pulse);
    RUN_TEST(test_busy_during_settling);
    RUN_TEST(test_not_busy_after_complete);

    // Interlock safety
    RUN_TEST(test_cannot_send_pulse_while_busy);
    RUN_TEST(test_cannot_send_pulse_when_relay_active);

    // Runtime tracking
    RUN_TEST(test_runtime_zero_when_idle);
    RUN_TEST(test_runtime_tracks_extending);
    RUN_TEST(test_runtime_zero_after_stop);

    // Edge cases
    RUN_TEST(test_update_without_time_provider_safe);

    return UNITY_END();
}
