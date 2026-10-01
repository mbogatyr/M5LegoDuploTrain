#include <unity.h>

#include "ShakeDetector.h"

void setUp(void) {}
void tearDown(void) {}

void test_resting_is_not_a_shake(void) {
    ShakeDetector shake;
    TEST_ASSERT_FALSE(shake.update(0, 0.0f, 1.0f, 0.0f));
    TEST_ASSERT_FALSE(shake.update(20, 0.3f, 0.9f, 0.4f));
}

void test_ordinary_handling_is_not_a_shake(void) {
    ShakeDetector shake;
    TEST_ASSERT_FALSE(shake.update(0, 0.8f, 1.2f, 0.9f)); // about 1.7 g
}

void test_jolt_is_a_shake(void) {
    ShakeDetector shake;
    TEST_ASSERT_TRUE(shake.update(0, 1.5f, 1.5f, 1.0f)); // about 2.3 g
}

void test_one_shake_gives_one_event(void) {
    ShakeDetector shake;
    TEST_ASSERT_TRUE(shake.update(1000, 0.0f, 3.0f, 0.0f));
    TEST_ASSERT_FALSE(shake.update(1100, 0.0f, -3.0f, 0.0f));
    TEST_ASSERT_FALSE(shake.update(1000 + ShakeDetector::kCooldownMs - 1, 3.0f, 0.0f, 0.0f));
}

void test_next_shake_after_the_cooldown(void) {
    ShakeDetector shake;
    shake.update(1000, 0.0f, 3.0f, 0.0f);
    TEST_ASSERT_TRUE(shake.update(1000 + ShakeDetector::kCooldownMs, 0.0f, 3.0f, 0.0f));
}

void test_first_shake_right_after_start(void) {
    ShakeDetector shake;
    TEST_ASSERT_TRUE(shake.update(5, 0.0f, 0.0f, 3.0f));
}

void test_cooldown_survives_the_millis_rollover(void) {
    ShakeDetector shake;
    const uint32_t nearOverflow = 0xFFFFFF00u;
    shake.update(nearOverflow, 0.0f, 3.0f, 0.0f);
    TEST_ASSERT_FALSE(shake.update(nearOverflow + 500, 0.0f, 3.0f, 0.0f));
    TEST_ASSERT_TRUE(shake.update(nearOverflow + ShakeDetector::kCooldownMs, 0.0f, 3.0f, 0.0f));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_resting_is_not_a_shake);
    RUN_TEST(test_ordinary_handling_is_not_a_shake);
    RUN_TEST(test_jolt_is_a_shake);
    RUN_TEST(test_one_shake_gives_one_event);
    RUN_TEST(test_next_shake_after_the_cooldown);
    RUN_TEST(test_first_shake_right_after_start);
    RUN_TEST(test_cooldown_survives_the_millis_rollover);

    return UNITY_END();
}
