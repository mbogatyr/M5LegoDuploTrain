#include "TrainControls.h"

namespace {

using duplo::Color;
using duplo::Sound;

// The order KEY2 steps through the sounds. The horn and the brake are left
// out: a shake sounds the horn, and the brake plays when KEY1 stops the train.
constexpr Sound kSoundCycle[] = {
    Sound::Steam,
    Sound::StationDeparture,
    Sound::WaterRefill,
};
constexpr int kSoundCycleLength = sizeof(kSoundCycle) / sizeof(kSoundCycle[0]);

// The order a double press of KEY2 steps through the LED colours: only the
// basic ones, so a child can tell them apart. The train starts with Off.
constexpr Color kLightCycle[] = {
    Color::Off,
    Color::Red,
    Color::Blue,
    Color::Green,
    Color::White,
};
constexpr int kLightCycleLength = sizeof(kLightCycle) / sizeof(kLightCycle[0]);

Color nextInCycle(Color current) {
    for (int i = 0; i < kLightCycleLength; ++i) {
        if (kLightCycle[i] == current) {
            return kLightCycle[(i + 1) % kLightCycleLength];
        }
    }
    return kLightCycle[0];
}

TrainControls::Command motor(int power) {
    TrainControls::Command command;
    command.hasMotor = true;
    command.motorPower = power;
    return command;
}

} // namespace

TrainControls::Command TrainControls::toggleRun() {
    running_ = !running_;
    if (running_) {
        return motor(powerFor(level_));
    }

    Command command = motor(0);
    command.hasSound = true;
    command.sound = Sound::Brake;
    return command;
}

TrainControls::Command TrainControls::setLevel(int level) {
    if (level > kMaxLevel) {
        level = kMaxLevel;
    } else if (level < -kMaxLevel) {
        level = -kMaxLevel;
    }
    if (level == level_) {
        return Command();
    }
    level_ = level;
    return running_ ? motor(powerFor(level_)) : Command();
}

TrainControls::Command TrainControls::nextSound() {
    soundIndex_ = (soundIndex_ + 1) % kSoundCycleLength;
    Command command;
    command.hasSound = true;
    command.sound = kSoundCycle[soundIndex_];
    return command;
}

TrainControls::Command TrainControls::nextLight() {
    light_ = nextInCycle(light_);
    Command command;
    command.hasLight = true;
    command.light = light_;
    return command;
}

TrainControls::Command TrainControls::horn() {
    Command command;
    command.hasSound = true;
    command.sound = Sound::Horn;
    return command;
}

void TrainControls::reset() {
    running_ = false;
    light_ = Color::Off;
}

int TrainControls::powerFor(int level) {
    if (level > kMaxLevel) {
        level = kMaxLevel;
    } else if (level < -kMaxLevel) {
        level = -kMaxLevel;
    }
    return level >= 0 ? kLevelPower[level] : -kLevelPower[-level];
}

const char *TrainControls::label(Sound sound) {
    switch (sound) {
    case Sound::Brake:
        return "Brake";
    case Sound::StationDeparture:
        return "Depart";
    case Sound::WaterRefill:
        return "Water";
    case Sound::Horn:
        return "Horn";
    case Sound::Steam:
        return "Steam";
    }
    return "";
}

const char *TrainControls::label(Color color) {
    switch (color) {
    case Color::Off:
        return "Off";
    case Color::Pink:
        return "Pink";
    case Color::Purple:
        return "Purple";
    case Color::Blue:
        return "Blue";
    case Color::LightBlue:
        return "Light blue";
    case Color::Cyan:
        return "Cyan";
    case Color::Green:
        return "Green";
    case Color::Yellow:
        return "Yellow";
    case Color::Orange:
        return "Orange";
    case Color::Red:
        return "Red";
    case Color::White:
        return "White";
    }
    return "";
}
