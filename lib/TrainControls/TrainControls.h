#pragma once

#include <stdint.h>

#include "DuploProtocol.h"

// What the remote does with the train.
//
// The tilt of the stick sets a speed level from -kMaxLevel (full reverse) to
// kMaxLevel (full forward) at any time. KEY1 starts and stops the train:
// while it runs, the train follows the level; while it stands, the level is
// only a preview of how it would start. KEY2 plays the next sound, a double
// press of KEY2 switches the LED to the next colour, a shake sounds the horn.
//
// Like everything in lib/, it does not touch the hardware: every method
// returns what to send, main.cpp sends it.
class TrainControls {
  public:
    static constexpr int kMaxLevel = 5;
    // Motor power in percent for each speed level, from standing to full.
    // The train does not move below about 25 %. A power of 0 means stop:
    // main.cpp sends it as the brake.
    static constexpr int kLevelPower[kMaxLevel + 1] = {0, 30, 45, 60, 80, 100};

    // What to send to the train. Each part is optional; when several are
    // present, they are sent in the order motor, sound, light.
    struct Command {
        bool hasMotor = false;
        int motorPower = 0;

        bool hasSound = false;
        duplo::Sound sound = duplo::Sound::Horn;

        bool hasLight = false;
        duplo::Color light = duplo::Color::Off;
    };

    // KEY1: starts the train at the current level, or stops it. Stopping
    // plays the brake sound.
    Command toggleRun();

    // The level from the tilt, clamped to +-kMaxLevel. A running train gets
    // a motor command whenever the level changes.
    Command setLevel(int level);

    // KEY2: the next of steam, departure and water refill, in a cycle.
    Command nextSound();

    // KEY2 twice: the next LED colour: Off, Red, Blue, Green, White, in a
    // cycle.
    Command nextLight();

    // A shake of the stick.
    Command horn();

    // Marks the train as standing with its LED off, without a command: it
    // has just connected or disconnected. Keeps the level and the place in
    // the sound cycle.
    void reset();

    bool running() const { return running_; }
    int level() const { return level_; }
    duplo::Color lightColor() const { return light_; }

    static int powerFor(int level);
    static const char *label(duplo::Sound sound);
    static const char *label(duplo::Color color);

  private:
    bool running_ = false;
    int level_ = 0;
    int soundIndex_ = -1; // the last sound played, -1 before the first
    duplo::Color light_ = duplo::Color::Off;
};
