#include <unity.h>

#include "Repeats.h"

// Counts how many times the command is due between two moments, ticking
// every 20 ms like loop().
static int repeatsBetween(Repeats &repeats, uint32_t fromMs, uint32_t toMs) {
    int count = 0;
    for (uint32_t t = fromMs; t != toMs; t += 20) {
        if (repeats.due(t)) {
            ++count;
        }
    }
    return count;
}

void setUp(void) {}
void tearDown(void) {}

void test_nothing_is_due_before_a_start(void) {
    Repeats repeats;
    TEST_ASSERT_EQUAL_INT(0, repeatsBetween(repeats, 0, 10000));
}

void test_repeats_follow_the_schedule(void) {
    Repeats repeats;
    repeats.start(1000);
    for (int i = 0; i < Repeats::kCount; ++i) {
        const uint32_t at = 1000 + Repeats::kAtMs[i];
        TEST_ASSERT_FALSE(repeats.due(at - 1));
        TEST_ASSERT_TRUE(repeats.due(at));
        TEST_ASSERT_FALSE(repeats.due(at)); // reported once
    }
}

void test_repeats_end(void) {
    Repeats repeats;
    repeats.start(0);
    TEST_ASSERT_EQUAL_INT(Repeats::kCount, repeatsBetween(repeats, 0, 60000));
}

void test_cancel_ends_the_repeats(void) {
    Repeats repeats;
    repeats.start(0);
    TEST_ASSERT_TRUE(repeats.due(Repeats::kAtMs[0]));
    repeats.cancel();
    TEST_ASSERT_EQUAL_INT(0, repeatsBetween(repeats, 300, 10000));
}

void test_a_new_start_starts_over(void) {
    Repeats repeats;
    repeats.start(0);
    repeatsBetween(repeats, 0, 2000); // three of four done
    repeats.start(2000);
    TEST_ASSERT_EQUAL_INT(Repeats::kCount, repeatsBetween(repeats, 2000, 20000));
}

void test_missed_repeats_collapse_into_one(void) {
    Repeats repeats;
    repeats.start(0);
    TEST_ASSERT_TRUE(repeats.due(2000)); // the 200, 600 and 1500 ms repeats
    TEST_ASSERT_FALSE(repeats.due(2500));
    TEST_ASSERT_TRUE(repeats.due(3000));
    TEST_ASSERT_FALSE(repeats.due(10000));
}

void test_a_moment_before_the_start_is_not_due(void) {
    Repeats repeats;
    repeats.start(5000);
    TEST_ASSERT_FALSE(repeats.due(4400)); // a time taken before a blocking call
    TEST_ASSERT_TRUE(repeats.due(5200));
}

void test_schedule_survives_the_millis_rollover(void) {
    Repeats repeats;
    const uint32_t nearOverflow = 0xFFFFFF00u;
    repeats.start(nearOverflow);
    TEST_ASSERT_FALSE(repeats.due(nearOverflow + 100));
    TEST_ASSERT_TRUE(repeats.due(nearOverflow + 200)); // past zero
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_nothing_is_due_before_a_start);
    RUN_TEST(test_repeats_follow_the_schedule);
    RUN_TEST(test_repeats_end);
    RUN_TEST(test_cancel_ends_the_repeats);
    RUN_TEST(test_a_new_start_starts_over);
    RUN_TEST(test_missed_repeats_collapse_into_one);
    RUN_TEST(test_a_moment_before_the_start_is_not_due);
    RUN_TEST(test_schedule_survives_the_millis_rollover);

    return UNITY_END();
}
