#pragma once

#include <stdint.h>

// Notices a shake of the stick: a jolt well above gravity.
//
// One shake gives one event, however many peaks it has: after an event,
// further jolts are ignored for kCooldownMs.
//
// Like everything in lib/, it never touches hardware: the time and the
// acceleration in g go in, a yes or no comes out.
class ShakeDetector {
  public:
    static constexpr float kThresholdG = 2.2f;
    static constexpr uint32_t kCooldownMs = 1200;

    // True when this reading is the start of a shake.
    bool update(uint32_t nowMs, float ax, float ay, float az);

  private:
    bool hasShaken_ = false;
    uint32_t lastShakeMs_ = 0;
};
