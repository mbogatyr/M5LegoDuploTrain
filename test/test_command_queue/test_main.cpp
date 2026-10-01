#include <unity.h>

#include "CommandQueue.h"

using duplo::Message;

static const uint32_t kGap = CommandQueue::kGapMs;

// The value byte of a port output command: the power, the sound or the colour.
static uint8_t valueOf(const Message &message) { return message.bytes[7]; }

static uint8_t portOf(const Message &message) { return message.bytes[3]; }

void setUp(void) {}
void tearDown(void) {}

void test_empty_queue_gives_nothing(void) {
    CommandQueue queue;
    Message message;
    TEST_ASSERT_TRUE(queue.empty());
    TEST_ASSERT_FALSE(queue.next(0, message));
}

void test_first_message_goes_out_at_once(void) {
    CommandQueue queue;
    queue.put(duplo::motorPower(45));
    Message message;
    TEST_ASSERT_TRUE(queue.next(5000, message));
    TEST_ASSERT_EQUAL_UINT8(45, valueOf(message));
    TEST_ASSERT_TRUE(queue.empty());
}

void test_brake_and_sound_go_out_a_gap_apart(void) {
    // A KEY1 stop: the two commands that used to lose one of them.
    CommandQueue queue;
    queue.put(duplo::motorBrake());
    queue.put(duplo::playSound(duplo::Sound::Brake));

    Message message;
    TEST_ASSERT_TRUE(queue.next(1000, message));
    TEST_ASSERT_EQUAL_UINT8(duplo::kPortMotor, portOf(message));
    TEST_ASSERT_EQUAL_UINT8(0x7F, valueOf(message));

    TEST_ASSERT_FALSE(queue.next(1000, message));
    TEST_ASSERT_FALSE(queue.next(1000 + kGap - 1, message));
    TEST_ASSERT_TRUE(queue.next(1000 + kGap, message));
    TEST_ASSERT_EQUAL_UINT8(duplo::kPortSpeaker, portOf(message));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(duplo::Sound::Brake), valueOf(message));
}

void test_the_latest_message_per_port_wins(void) {
    CommandQueue queue;
    Message message;
    queue.put(duplo::motorPower(30));
    queue.next(0, message); // goes out
    queue.put(duplo::motorPower(45));
    queue.put(duplo::motorPower(60));
    queue.put(duplo::motorPower(80)); // the tilt swept through three levels
    TEST_ASSERT_TRUE(queue.next(kGap, message));
    TEST_ASSERT_EQUAL_UINT8(80, valueOf(message));
    TEST_ASSERT_FALSE(queue.next(2 * kGap, message));
}

void test_motor_goes_first_then_speaker_then_led(void) {
    CommandQueue queue;
    queue.put(duplo::ledColor(duplo::Color::Red));
    queue.put(duplo::playSound(duplo::Sound::Steam));
    queue.put(duplo::motorPower(30));

    const uint8_t expected[] = {duplo::kPortMotor, duplo::kPortSpeaker, duplo::kPortLed};
    uint32_t now = 0;
    for (uint8_t port : expected) {
        Message message;
        TEST_ASSERT_TRUE(queue.next(now, message));
        TEST_ASSERT_EQUAL_UINT8(port, portOf(message));
        now += kGap;
    }
    TEST_ASSERT_TRUE(queue.empty());
}

void test_gap_counts_from_the_last_message(void) {
    CommandQueue queue;
    Message message;
    queue.put(duplo::motorPower(30));
    queue.next(0, message);
    // A long quiet spell: the next message goes out as soon as it is put.
    queue.put(duplo::playSound(duplo::Sound::Steam));
    TEST_ASSERT_TRUE(queue.next(10000, message));
}

void test_a_time_before_the_last_message_is_too_early(void) {
    CommandQueue queue;
    Message message;
    queue.put(duplo::motorPower(30));
    queue.next(5000, message);
    queue.put(duplo::motorPower(45));
    TEST_ASSERT_FALSE(queue.next(4990, message));
}

void test_retry_puts_a_failed_message_back(void) {
    CommandQueue queue;
    Message message;
    queue.put(duplo::motorBrake());
    queue.next(0, message);
    queue.retry(message);
    TEST_ASSERT_FALSE(queue.next(kGap - 1, message)); // still the usual gap
    TEST_ASSERT_TRUE(queue.next(kGap, message));
    TEST_ASSERT_EQUAL_UINT8(0x7F, valueOf(message));
}

void test_retry_does_not_override_a_newer_message(void) {
    CommandQueue queue;
    Message failed;
    queue.put(duplo::motorPower(30));
    queue.next(0, failed);
    queue.put(duplo::motorPower(60));
    queue.retry(failed);
    Message message;
    TEST_ASSERT_TRUE(queue.next(kGap, message));
    TEST_ASSERT_EQUAL_UINT8(60, valueOf(message));
}

void test_clear_drops_everything(void) {
    CommandQueue queue;
    queue.put(duplo::motorPower(30));
    queue.put(duplo::ledColor(duplo::Color::Blue));
    queue.clear();
    Message message;
    TEST_ASSERT_TRUE(queue.empty());
    TEST_ASSERT_FALSE(queue.next(10000, message));
}

void test_other_messages_wait_last(void) {
    CommandQueue queue;
    queue.put(duplo::ledColorMode()); // not a port output command
    queue.put(duplo::ledColor(duplo::Color::Green));
    Message message;
    TEST_ASSERT_TRUE(queue.next(0, message));
    TEST_ASSERT_EQUAL_UINT8(0x81, message.bytes[2]);
    TEST_ASSERT_TRUE(queue.next(kGap, message));
    TEST_ASSERT_EQUAL_UINT8(0x41, message.bytes[2]);
}

void test_gap_survives_the_millis_rollover(void) {
    CommandQueue queue;
    Message message;
    const uint32_t nearOverflow = 0xFFFFFFF0u;
    queue.put(duplo::motorPower(30));
    queue.next(nearOverflow, message);
    queue.put(duplo::motorPower(45));
    TEST_ASSERT_FALSE(queue.next(nearOverflow + kGap - 1, message));
    TEST_ASSERT_TRUE(queue.next(nearOverflow + kGap, message)); // past zero
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_queue_gives_nothing);
    RUN_TEST(test_first_message_goes_out_at_once);
    RUN_TEST(test_brake_and_sound_go_out_a_gap_apart);
    RUN_TEST(test_the_latest_message_per_port_wins);
    RUN_TEST(test_motor_goes_first_then_speaker_then_led);
    RUN_TEST(test_gap_counts_from_the_last_message);
    RUN_TEST(test_a_time_before_the_last_message_is_too_early);
    RUN_TEST(test_retry_puts_a_failed_message_back);
    RUN_TEST(test_retry_does_not_override_a_newer_message);
    RUN_TEST(test_clear_drops_everything);
    RUN_TEST(test_other_messages_wait_last);
    RUN_TEST(test_gap_survives_the_millis_rollover);

    return UNITY_END();
}
