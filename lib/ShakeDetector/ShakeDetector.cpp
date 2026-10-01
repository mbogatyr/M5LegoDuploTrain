#include "ShakeDetector.h"

bool ShakeDetector::update(uint32_t nowMs, float ax, float ay, float az) {
    // Compared squared, so no square root is needed.
    const float squared = ax * ax + ay * ay + az * az;
    if (squared < kThresholdG * kThresholdG) {
        return false;
    }
    // Unsigned subtraction handles the millis() rollover correctly.
    if (hasShaken_ && nowMs - lastShakeMs_ < kCooldownMs) {
        return false;
    }
    hasShaken_ = true;
    lastShakeMs_ = nowMs;
    return true;
}
