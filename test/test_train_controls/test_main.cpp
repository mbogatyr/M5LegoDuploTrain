#include <unity.h>

#include "TrainControls.h"

using duplo::Color;
using duplo::Sound;
using Command = TrainControls::Command;

static void assertSound(Sound expected, const Command &command) {
    TEST_ASSERT_TRUE(command.hasSound);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(command.sound));
}

static void assertMotor(int power, const Command &command) {
    TEST_ASSERT_TRUE(command.hasMotor);
    TEST_ASSERT_EQUAL_INT(power, command.motorPower);
}

static void assertNothing(const Command &command) {
    TEST_ASSERT_FALSE(command.hasMotor);
    TEST_ASSERT_FALSE(command.hasSound);
    TEST_ASSERT_FALSE(command.hasLight);
}

void setUp(void) {}
void tearDown(void) {}

void test_starts_standing_level_with_the_light_off(void) {
    TrainControls controls;
    TEST_ASSERT_FALSE(controls.running());
    TEST_ASSERT_EQUAL_INT(0, controls.level());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Color::Off),
                            static_cast<uint8_t>(controls.lightColor()));
}

void test_power_per_level(void) {
    TEST_ASSERT_EQUAL_INT(0, TrainControls::powerFor(0));
    TEST_ASSERT_EQUAL_INT(TrainControls::kLevelPower[3], TrainControls::powerFor(3));
    TEST_ASSERT_EQUAL_INT(-TrainControls::kLevelPower[2], TrainControls::powerFor(-2));
    TEST_ASSERT_EQUAL_INT(100, TrainControls::powerFor(TrainControls::kMaxLevel));
    TEST_ASSERT_EQUAL_INT(-100, TrainControls::powerFor(-TrainControls::kMaxLevel - 3));
}

void test_tilt_while_standing_only_previews(void) {
    TrainControls controls;
    assertNothing(controls.setLevel(3));
    TEST_ASSERT_EQUAL_INT(3, controls.level());
    TEST_ASSERT_FALSE(controls.running());
}

void test_start_goes_at_the_previewed_level(void) {
    TrainControls controls;
    controls.setLevel(-2);
    const Command command = controls.toggleRun();
    TEST_ASSERT_TRUE(controls.running());
    assertMotor(TrainControls::powerFor(-2), command);
    TEST_ASSERT_FALSE(command.hasSound);
}

void test_start_while_level_waits_for_the_tilt(void) {
    TrainControls controls;
    assertMotor(0, controls.toggleRun());
    TEST_ASSERT_TRUE(controls.running());
    assertMotor(TrainControls::powerFor(1), controls.setLevel(1));
}

void test_running_train_follows_the_level(void) {
    TrainControls controls;
    controls.toggleRun();
    assertMotor(TrainControls::powerFor(2), controls.setLevel(2));
    assertMotor(TrainControls::powerFor(4), controls.setLevel(4));
    // Through zero to reverse: stands for a moment, without a brake sound.
    const Command throughZero = controls.setLevel(0);
    assertMotor(0, throughZero);
    TEST_ASSERT_FALSE(throughZero.hasSound);
    assertMotor(TrainControls::powerFor(-1), controls.setLevel(-1));
}

void test_unchanged_level_sends_nothing(void) {
    TrainControls controls;
    controls.toggleRun();
    controls.setLevel(2);
    assertNothing(controls.setLevel(2));
}

void test_level_is_clamped(void) {
    TrainControls controls;
    controls.setLevel(9);
    TEST_ASSERT_EQUAL_INT(TrainControls::kMaxLevel, controls.level());
    controls.setLevel(-9);
    TEST_ASSERT_EQUAL_INT(-TrainControls::kMaxLevel, controls.level());
}

void test_stopping_a_moving_train_brakes(void) {
    TrainControls controls;
    controls.setLevel(3);
    controls.toggleRun();
    const Command command = controls.toggleRun();
    TEST_ASSERT_FALSE(controls.running());
    assertMotor(0, command);
    assertSound(Sound::Brake, command);
    TEST_ASSERT_EQUAL_INT(3, controls.level()); // the tilt is still there
}

void test_stopping_at_level_zero_brakes_too(void) {
    TrainControls controls;
    controls.toggleRun();
    const Command command = controls.toggleRun();
    assertMotor(0, command);
    assertSound(Sound::Brake, command);
}

void test_sounds_cycle_without_horn_and_brake_and_keep_the_motion(void) {
    TrainControls controls;
    controls.setLevel(2);
    controls.toggleRun();

    const Sound order[] = {Sound::Steam, Sound::StationDeparture, Sound::WaterRefill,
                           Sound::Steam};
    for (Sound expected : order) {
        const Command command = controls.nextSound();
        assertSound(expected, command);
        TEST_ASSERT_FALSE(command.hasMotor);
        TEST_ASSERT_FALSE(command.hasLight);
    }
    TEST_ASSERT_TRUE(controls.running());
    TEST_ASSERT_EQUAL_INT(2, controls.level());
}

void test_light_cycles_through_the_basic_colours_and_off(void) {
    TrainControls controls;
    const Color order[] = {Color::Red,   Color::Blue, Color::Green,
                           Color::White, Color::Off,  Color::Red};
    for (Color expected : order) {
        const Command command = controls.nextLight();
        TEST_ASSERT_TRUE(command.hasLight);
        TEST_ASSERT_FALSE(command.hasMotor);
        TEST_ASSERT_FALSE(command.hasSound);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                                static_cast<uint8_t>(command.light));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                                static_cast<uint8_t>(controls.lightColor()));
    }
}

void test_horn_does_not_move_the_sound_cycle(void) {
    TrainControls controls;
    controls.nextSound(); // Steam
    assertSound(Sound::Horn, controls.horn());
    assertSound(Sound::StationDeparture, controls.nextSound());
}

void test_reset_stops_and_turns_the_light_off_without_a_command(void) {
    TrainControls controls;
    controls.nextLight();
    controls.nextSound();
    controls.setLevel(2);
    controls.toggleRun();

    controls.reset();

    TEST_ASSERT_FALSE(controls.running());
    TEST_ASSERT_EQUAL_INT(2, controls.level());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Color::Off),
                            static_cast<uint8_t>(controls.lightColor()));
    assertNothing(controls.setLevel(3)); // standing again: only a preview
    assertSound(Sound::StationDeparture, controls.nextSound()); // the cycle goes on
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Color::Red),
                            static_cast<uint8_t>(controls.nextLight().light));
}

void test_labels(void) {
    TEST_ASSERT_EQUAL_STRING("Depart", TrainControls::label(Sound::StationDeparture));
    TEST_ASSERT_EQUAL_STRING("Water", TrainControls::label(Sound::WaterRefill));
    TEST_ASSERT_EQUAL_STRING("Light blue", TrainControls::label(Color::LightBlue));
    TEST_ASSERT_EQUAL_STRING("Off", TrainControls::label(Color::Off));
}

int main(int, char **) {
    UNITY_BEGIN();

    RUN_TEST(test_starts_standing_level_with_the_light_off);
    RUN_TEST(test_power_per_level);
    RUN_TEST(test_tilt_while_standing_only_previews);
    RUN_TEST(test_start_goes_at_the_previewed_level);
    RUN_TEST(test_start_while_level_waits_for_the_tilt);
    RUN_TEST(test_running_train_follows_the_level);
    RUN_TEST(test_unchanged_level_sends_nothing);
    RUN_TEST(test_level_is_clamped);
    RUN_TEST(test_stopping_a_moving_train_brakes);
    RUN_TEST(test_stopping_at_level_zero_brakes_too);
    RUN_TEST(test_sounds_cycle_without_horn_and_brake_and_keep_the_motion);
    RUN_TEST(test_light_cycles_through_the_basic_colours_and_off);
    RUN_TEST(test_horn_does_not_move_the_sound_cycle);
    RUN_TEST(test_reset_stops_and_turns_the_light_off_without_a_command);
    RUN_TEST(test_labels);

    return UNITY_END();
}
