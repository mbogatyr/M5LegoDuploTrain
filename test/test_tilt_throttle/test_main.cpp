#include <math.h>
#include <unity.h>

#include "TiltThrottle.h"

// Feeds a steady pose, pitched from upright towards the screen facing up,
// long enough for the filter to settle.
static void hold(TiltThrottle &throttle, float pitchDeg, float g = 1.0f) {
    const float rad = pitchDeg / 57.2957795f;
    for (int i = 0; i < 60; ++i) {
        throttle.update(0.0f, g * cosf(rad), g * sinf(rad));
    }
}

static const float kNeutral = TiltThrottle::kDefaultNeutralDeg;
static const float kDead = TiltThrottle::kDeadZoneDeg;
static const float kStep = TiltThrottle::kStepDeg;

void setUp(void) {}
void tearDown(void) {}

void test_pitch_runs_from_upright_to_screen_up(void) {
    TiltThrottle throttle;
    hold(throttle, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, throttle.pitchDeg());
    hold(throttle, 90.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, throttle.pitchDeg());
}

void test_neutral_pose_stands(void) {
    TiltThrottle throttle;
    hold(throttle, kNeutral);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
}

void test_dead_zone_stands(void) {
    TiltThrottle throttle;
    hold(throttle, kNeutral + kDead - 1.0f);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
    hold(throttle, kNeutral - kDead + 1.0f);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
}

void test_tipping_away_goes_forward_a_level_per_step(void) {
    TiltThrottle throttle;
    for (int level = 1; level <= TiltThrottle::kMaxLevel; ++level) {
        // The middle of each level's band, clear of the hysteresis.
        hold(throttle, kNeutral + kDead + (level - 0.5f) * kStep);
        TEST_ASSERT_EQUAL_INT(level, throttle.level());
    }
}

void test_tipping_towards_goes_backward(void) {
    TiltThrottle throttle;
    hold(throttle, kNeutral - kDead - 1.5f * kStep);
    TEST_ASSERT_EQUAL_INT(-2, throttle.level());
}

void test_level_stops_at_the_maximum(void) {
    TiltThrottle throttle;
    hold(throttle, 89.0f);
    TEST_ASSERT_EQUAL_INT(TiltThrottle::kMaxLevel, throttle.level());
    hold(throttle, -60.0f);
    TEST_ASSERT_EQUAL_INT(-TiltThrottle::kMaxLevel, throttle.level());
}

void test_hysteresis_holds_the_level_near_a_boundary(void) {
    TiltThrottle throttle;
    const float boundary = kNeutral + kDead + kStep; // between levels 1 and 2
    hold(throttle, boundary - 0.5f * kStep);
    TEST_ASSERT_EQUAL_INT(1, throttle.level());

    // Just past the boundary is not enough to go up...
    hold(throttle, boundary + 0.1f * kStep);
    TEST_ASSERT_EQUAL_INT(1, throttle.level());
    // ...past it by more than the hysteresis is.
    hold(throttle, boundary + 0.5f * kStep);
    TEST_ASSERT_EQUAL_INT(2, throttle.level());

    // And on the way down, just below the boundary keeps level 2.
    hold(throttle, boundary - 0.1f * kStep);
    TEST_ASSERT_EQUAL_INT(2, throttle.level());
    hold(throttle, boundary - 0.5f * kStep);
    TEST_ASSERT_EQUAL_INT(1, throttle.level());
}

void test_hysteresis_around_the_dead_zone(void) {
    TiltThrottle throttle;
    hold(throttle, kNeutral);
    hold(throttle, kNeutral + kDead + 0.1f * kStep);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
    hold(throttle, kNeutral + kDead + 0.5f * kStep);
    TEST_ASSERT_EQUAL_INT(1, throttle.level());
    hold(throttle, kNeutral + kDead - 0.1f * kStep);
    TEST_ASSERT_EQUAL_INT(1, throttle.level());
    hold(throttle, kNeutral);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
}

void test_shakes_and_bumps_are_ignored(void) {
    TiltThrottle throttle;
    hold(throttle, kNeutral);
    // A violent reading pointing at full forward, but far from 1 g.
    hold(throttle, 89.0f, 2.5f);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
    hold(throttle, 89.0f, 0.2f);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
}

void test_recenter_makes_the_current_pose_neutral(void) {
    TiltThrottle throttle;
    const float pose = kNeutral + kDead + 4.5f * kStep;
    hold(throttle, pose);
    TEST_ASSERT_EQUAL_INT(TiltThrottle::kMaxLevel, throttle.level());

    throttle.recenter();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, pose, throttle.neutralDeg());
    TEST_ASSERT_EQUAL_INT(0, throttle.level());

    hold(throttle, pose - kDead - 0.5f * kStep);
    TEST_ASSERT_EQUAL_INT(-1, throttle.level());
}

void test_recenter_before_any_reading_keeps_the_neutral(void) {
    TiltThrottle throttle(25.0f);
    throttle.recenter();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, throttle.neutralDeg());
}

void test_neutral_across_the_wrap_around(void) {
    // Neutral almost upside down: angles either side of +-180 are close.
    TiltThrottle throttle(178.0f);
    hold(throttle, -178.0f);
    TEST_ASSERT_EQUAL_INT(0, throttle.level());
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_pitch_runs_from_upright_to_screen_up);
    RUN_TEST(test_neutral_pose_stands);
    RUN_TEST(test_dead_zone_stands);
    RUN_TEST(test_tipping_away_goes_forward_a_level_per_step);
    RUN_TEST(test_tipping_towards_goes_backward);
    RUN_TEST(test_level_stops_at_the_maximum);
    RUN_TEST(test_hysteresis_holds_the_level_near_a_boundary);
    RUN_TEST(test_hysteresis_around_the_dead_zone);
    RUN_TEST(test_shakes_and_bumps_are_ignored);
    RUN_TEST(test_recenter_makes_the_current_pose_neutral);
    RUN_TEST(test_recenter_before_any_reading_keeps_the_neutral);
    RUN_TEST(test_neutral_across_the_wrap_around);

    return UNITY_END();
}
