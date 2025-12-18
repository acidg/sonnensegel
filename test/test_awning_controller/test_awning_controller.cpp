#include <unity.h>
#include "awning_state_machine.h"

// Test fixtures
static PositionTrackerCore* tracker;
static AwningStateMachine* awning;
static unsigned long mockTime;

void setUp() {
    mockTime = 0;
    tracker = new PositionTrackerCore();
    awning = new AwningStateMachine(*tracker, nullptr);  // nullptr = no hardware
}

void tearDown() {
    delete awning;
    delete tracker;
}

// =============================================================================
// State Machine: Initial State Tests
// =============================================================================

void test_initial_state_is_idle() {
    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
}

void test_initial_position_is_zero() {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, awning->getCurrentPosition());
}

void test_startup_does_not_move_motor() {
    // Set position to 50% but don't call setTarget
    awning->setCurrentPosition(50.0f);

    // Update multiple times
    for (int i = 0; i < 10; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    // Should still be IDLE - no movement
    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FALSE(awning->isMoving());
}

// =============================================================================
// State Machine: IDLE -> EXTENDING Transition
// =============================================================================

void test_set_target_higher_transitions_to_extending() {
    awning->setCurrentPosition(0.0f);
    awning->setTarget(50.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());
    TEST_ASSERT_TRUE(awning->isMoving());
}

void test_set_target_100_transitions_to_extending() {
    awning->setCurrentPosition(0.0f);
    awning->setTarget(100.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());
}

// =============================================================================
// State Machine: IDLE -> RETRACTING Transition
// =============================================================================

void test_set_target_lower_transitions_to_retracting() {
    awning->setCurrentPosition(50.0f);
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());
    TEST_ASSERT_TRUE(awning->isMoving());
}

void test_set_target_0_transitions_to_retracting() {
    awning->setCurrentPosition(100.0f);
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());
}

// =============================================================================
// State Machine: No Transition When Target Within Tolerance
// =============================================================================

void test_set_target_within_tolerance_stays_idle() {
    awning->setCurrentPosition(50.0f);
    awning->setTarget(50.5f);  // Within 1% tolerance

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FALSE(awning->isMoving());
}

void test_set_target_same_position_stays_idle() {
    awning->setCurrentPosition(50.0f);
    awning->setTarget(50.0f);

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
}

// =============================================================================
// State Machine: EXTENDING -> IDLE on Target Reached
// =============================================================================

void test_extending_to_idle_when_target_reached() {
    tracker->setTravelTime(10000);  // 10 seconds for full travel
    awning->setCurrentPosition(0.0f);
    awning->setTarget(10.0f);  // Target 10%

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());

    // Simulate time passing - 10% of 10000ms = 1000ms
    for (int i = 0; i < 15; i++) {  // 1500ms total
        mockTime += 100;
        awning->update(mockTime);
    }

    // Should have reached target and transitioned to IDLE
    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FALSE(awning->isMoving());
}

// =============================================================================
// State Machine: RETRACTING -> IDLE on Target Reached
// =============================================================================

void test_retracting_to_idle_when_target_reached() {
    tracker->setTravelTime(10000);
    awning->setCurrentPosition(20.0f);
    awning->setTarget(10.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());

    // Simulate time passing
    for (int i = 0; i < 15; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
}

// =============================================================================
// Oscillation Prevention: IDLE Ignores Position Drift
// =============================================================================

void test_idle_ignores_position_overshoot() {
    tracker->setTravelTime(10000);
    awning->setCurrentPosition(0.0f);
    awning->setTarget(50.0f);

    // Move to target
    for (int i = 0; i < 60; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    // Should be at IDLE now
    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());

    // Manually simulate overshoot by setting position beyond target
    tracker->setCurrentPosition(52.0f);  // Overshot by 2%

    // Update multiple times - should NOT start retracting
    for (int i = 0; i < 10; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    // CRITICAL: Should still be IDLE, not RETRACTING
    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FALSE(awning->isMoving());
}

void test_idle_ignores_position_undershoot() {
    tracker->setTravelTime(10000);
    awning->setCurrentPosition(100.0f);
    awning->setTarget(50.0f);

    // Move to target
    for (int i = 0; i < 60; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());

    // Simulate undershoot
    tracker->setCurrentPosition(48.0f);

    for (int i = 0; i < 10; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    // Should still be IDLE
    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
}

// =============================================================================
// Stop Command Tests
// =============================================================================

void test_stop_transitions_to_idle() {
    awning->setCurrentPosition(0.0f);
    awning->setTarget(100.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());

    awning->stop(PIN_RELAY_EXTEND);

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FALSE(awning->isMoving());
}

void test_stop_both_transitions_to_idle() {
    awning->setCurrentPosition(0.0f);
    awning->setTarget(100.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());

    awning->stopBoth();

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
}

void test_stop_from_retracting() {
    awning->setCurrentPosition(100.0f);
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());

    awning->stop(PIN_RELAY_RETRACT);

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
}

// =============================================================================
// Direction Change Tests
// =============================================================================

void test_direction_change_extending_to_retracting() {
    awning->setCurrentPosition(50.0f);
    awning->setTarget(100.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());

    // Change direction
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());
}

void test_direction_change_retracting_to_extending() {
    awning->setCurrentPosition(50.0f);
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());

    // Change direction
    awning->setTarget(100.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());
}

// =============================================================================
// Relay Tracking Tests
// =============================================================================

void test_last_movement_relay_for_extending() {
    awning->setCurrentPosition(0.0f);
    awning->setTarget(100.0f);

    TEST_ASSERT_EQUAL(PIN_RELAY_EXTEND, awning->getLastMovementRelay());
}

void test_last_movement_relay_for_retracting() {
    awning->setCurrentPosition(100.0f);
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(PIN_RELAY_RETRACT, awning->getLastMovementRelay());
}

// =============================================================================
// Limit Tests
// =============================================================================

void test_stops_at_max_position() {
    tracker->setTravelTime(10000);
    awning->setCurrentPosition(95.0f);
    awning->setTarget(100.0f);

    // Run until position maxes out
    for (int i = 0; i < 20; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, awning->getCurrentPosition());
}

void test_stops_at_min_position() {
    tracker->setTravelTime(10000);
    awning->setCurrentPosition(5.0f);
    awning->setTarget(0.0f);

    for (int i = 0; i < 20; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, awning->getCurrentPosition());
}

// =============================================================================
// New Command After Stop
// =============================================================================

void test_new_target_after_stop_works() {
    awning->setCurrentPosition(50.0f);
    awning->setTarget(100.0f);
    awning->stop(PIN_RELAY_EXTEND);

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());

    // New command should work
    awning->setTarget(0.0f);

    TEST_ASSERT_EQUAL(AWNING_RETRACTING, awning->getState());
}

void test_new_target_after_reaching_target_works() {
    tracker->setTravelTime(10000);
    awning->setCurrentPosition(0.0f);
    awning->setTarget(10.0f);

    // Reach target
    for (int i = 0; i < 20; i++) {
        mockTime += 100;
        awning->update(mockTime);
    }

    TEST_ASSERT_EQUAL(AWNING_IDLE, awning->getState());

    // New command
    awning->setTarget(50.0f);

    TEST_ASSERT_EQUAL(AWNING_EXTENDING, awning->getState());
}

// =============================================================================
// Test Runner
// =============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Initial state tests
    RUN_TEST(test_initial_state_is_idle);
    RUN_TEST(test_initial_position_is_zero);
    RUN_TEST(test_startup_does_not_move_motor);

    // IDLE -> EXTENDING
    RUN_TEST(test_set_target_higher_transitions_to_extending);
    RUN_TEST(test_set_target_100_transitions_to_extending);

    // IDLE -> RETRACTING
    RUN_TEST(test_set_target_lower_transitions_to_retracting);
    RUN_TEST(test_set_target_0_transitions_to_retracting);

    // No transition within tolerance
    RUN_TEST(test_set_target_within_tolerance_stays_idle);
    RUN_TEST(test_set_target_same_position_stays_idle);

    // Target reached transitions
    RUN_TEST(test_extending_to_idle_when_target_reached);
    RUN_TEST(test_retracting_to_idle_when_target_reached);

    // Oscillation prevention (CRITICAL tests)
    RUN_TEST(test_idle_ignores_position_overshoot);
    RUN_TEST(test_idle_ignores_position_undershoot);

    // Stop command
    RUN_TEST(test_stop_transitions_to_idle);
    RUN_TEST(test_stop_both_transitions_to_idle);
    RUN_TEST(test_stop_from_retracting);

    // Direction change
    RUN_TEST(test_direction_change_extending_to_retracting);
    RUN_TEST(test_direction_change_retracting_to_extending);

    // Relay tracking
    RUN_TEST(test_last_movement_relay_for_extending);
    RUN_TEST(test_last_movement_relay_for_retracting);

    // Limits
    RUN_TEST(test_stops_at_max_position);
    RUN_TEST(test_stops_at_min_position);

    // New commands after stop
    RUN_TEST(test_new_target_after_stop_works);
    RUN_TEST(test_new_target_after_reaching_target_works);

    return UNITY_END();
}
