#include <unity.h>

#include "DuploProtocol.h"

using duplo::Message;

// Compares a message with the bytes expected on the wire, length included.
static void assertBytes(const uint8_t *expected, size_t expectedLength, const Message &actual) {
    TEST_ASSERT_EQUAL_UINT(expectedLength, actual.length);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual.bytes, expectedLength);
}

#define ASSERT_MESSAGE(actual, ...)                                                      \
    do {                                                                                 \
        const uint8_t expected[] = {__VA_ARGS__};                                        \
        assertBytes(expected, sizeof(expected), (actual));                               \
    } while (0)

void setUp(void) {}
void tearDown(void) {}

void test_motor_forward(void) {
    ASSERT_MESSAGE(duplo::motorPower(50), 0x08, 0x00, 0x81, 0x00, 0x11, 0x51, 0x00, 0x32);
}

void test_motor_reverse_is_a_signed_byte(void) {
    ASSERT_MESSAGE(duplo::motorPower(-50), 0x08, 0x00, 0x81, 0x00, 0x11, 0x51, 0x00, 0xCE);
}

void test_motor_stop(void) {
    ASSERT_MESSAGE(duplo::motorPower(0), 0x08, 0x00, 0x81, 0x00, 0x11, 0x51, 0x00, 0x00);
}

void test_motor_brake(void) {
    ASSERT_MESSAGE(duplo::motorBrake(), 0x08, 0x00, 0x81, 0x00, 0x11, 0x51, 0x00, 0x7F);
}

void test_motor_power_is_clamped(void) {
    TEST_ASSERT_EQUAL_HEX8(100, duplo::motorPower(120).bytes[7]);
    TEST_ASSERT_EQUAL_HEX8(0x9C, duplo::motorPower(-120).bytes[7]); // -100
}

void test_play_sound(void) {
    ASSERT_MESSAGE(duplo::playSound(duplo::Sound::Horn), 0x08, 0x00, 0x81, 0x01, 0x11, 0x51,
                   0x01, 0x09);
}

void test_sound_numbers(void) {
    TEST_ASSERT_EQUAL_UINT8(3, duplo::playSound(duplo::Sound::Brake).bytes[7]);
    TEST_ASSERT_EQUAL_UINT8(5, duplo::playSound(duplo::Sound::StationDeparture).bytes[7]);
    TEST_ASSERT_EQUAL_UINT8(7, duplo::playSound(duplo::Sound::WaterRefill).bytes[7]);
    TEST_ASSERT_EQUAL_UINT8(10, duplo::playSound(duplo::Sound::Steam).bytes[7]);
}

void test_led_color(void) {
    ASSERT_MESSAGE(duplo::ledColor(duplo::Color::Red), 0x08, 0x00, 0x81, 0x11, 0x11, 0x51, 0x00,
                   0x09);
}

void test_speaker_sound_mode(void) {
    ASSERT_MESSAGE(duplo::speakerSoundMode(), 0x0A, 0x00, 0x41, 0x01, 0x01, 0x01, 0x00, 0x00,
                   0x00, 0x01);
}

void test_led_color_mode(void) {
    ASSERT_MESSAGE(duplo::ledColorMode(), 0x0A, 0x00, 0x41, 0x11, 0x00, 0x01, 0x00, 0x00, 0x00,
                   0x00);
}

void test_length_byte_matches_the_message_length(void) {
    const Message messages[] = {
        duplo::motorPower(10),
        duplo::motorBrake(),
        duplo::playSound(duplo::Sound::Steam),
        duplo::ledColor(duplo::Color::Blue),
        duplo::speakerSoundMode(),
        duplo::ledColorMode(),
    };
    for (const Message &message : messages) {
        TEST_ASSERT_EQUAL_UINT8(message.length, message.bytes[0]);
    }
}

void test_train_advertisement_is_recognised(void) {
    const uint8_t data[] = {0x97, 0x03, 0x00, 0x20, 0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE(duplo::isTrainAdvertisement(data, sizeof(data)));
}

void test_other_lego_hub_is_ignored(void) {
    const uint8_t cityHub[] = {0x97, 0x03, 0x00, 0x41, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(duplo::isTrainAdvertisement(cityHub, sizeof(cityHub)));
}

void test_other_company_is_ignored(void) {
    const uint8_t data[] = {0x4C, 0x00, 0x00, 0x20, 0x00};
    TEST_ASSERT_FALSE(duplo::isTrainAdvertisement(data, sizeof(data)));
}

void test_short_manufacturer_data_is_ignored(void) {
    const uint8_t data[] = {0x97, 0x03, 0x00};
    TEST_ASSERT_FALSE(duplo::isTrainAdvertisement(data, sizeof(data)));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_motor_forward);
    RUN_TEST(test_motor_reverse_is_a_signed_byte);
    RUN_TEST(test_motor_stop);
    RUN_TEST(test_motor_brake);
    RUN_TEST(test_motor_power_is_clamped);
    RUN_TEST(test_play_sound);
    RUN_TEST(test_sound_numbers);
    RUN_TEST(test_led_color);
    RUN_TEST(test_speaker_sound_mode);
    RUN_TEST(test_led_color_mode);
    RUN_TEST(test_length_byte_matches_the_message_length);
    RUN_TEST(test_train_advertisement_is_recognised);
    RUN_TEST(test_other_lego_hub_is_ignored);
    RUN_TEST(test_other_company_is_ignored);
    RUN_TEST(test_short_manufacturer_data_is_ignored);

    return UNITY_END();
}
