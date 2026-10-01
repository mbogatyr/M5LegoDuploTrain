#include "TiltThrottle.h"

#include <math.h>

namespace {

constexpr float kDegPerRad = 57.2957795f;

// Readings whose length is outside this band are not gravity alone.
constexpr float kMinG = 0.75f;
constexpr float kMaxG = 1.25f;

} // namespace

float TiltThrottle::wrap180(float deg) {
    while (deg > 180.0f) {
        deg -= 360.0f;
    }
    while (deg < -180.0f) {
        deg += 360.0f;
    }
    return deg;
}

float TiltThrottle::positionFor(float deg) {
    const float magnitude = fabsf(deg);
    if (magnitude < kDeadZoneDeg) {
        return deg / kDeadZoneDeg; // within (-1, 1)
    }
    const float position = 1.0f + (magnitude - kDeadZoneDeg) / kStepDeg;
    return deg > 0 ? position : -position;
}

void TiltThrottle::update(float ax, float ay, float az) {
    const float length = sqrtf(ax * ax + ay * ay + az * az);
    if (length < kMinG || length > kMaxG) {
        return;
    }
    // Tipping the top edge turns gravity around X, between Y and Z.
    const float y = ay / length;
    const float z = az / length;
    if (!hasReading_) {
        gy_ = y;
        gz_ = z;
        hasReading_ = true;
    } else {
        gy_ += (y - gy_) * kSmoothing;
        gz_ += (z - gz_) * kSmoothing;
    }
    pitchDeg_ = atan2f(gz_, gy_) * kDegPerRad;

    // The level holds while the position stays within its band widened by
    // the hysteresis; outside it, the level is the whole part of the position.
    const float position = positionFor(wrap180(pitchDeg_ - neutralDeg_));
    float low;
    float high;
    if (level_ > 0) {
        low = level_ - kHysteresis;
        high = level_ + 1 + kHysteresis;
    } else if (level_ < 0) {
        low = level_ - 1 - kHysteresis;
        high = level_ + kHysteresis;
    } else {
        low = -1 - kHysteresis;
        high = 1 + kHysteresis;
    }
    if (position <= low || position >= high) {
        int level = static_cast<int>(position); // towards zero
        if (level > kMaxLevel) {
            level = kMaxLevel;
        } else if (level < -kMaxLevel) {
            level = -kMaxLevel;
        }
        level_ = level;
    }
}

void TiltThrottle::recenter() {
    if (!hasReading_) {
        return;
    }
    neutralDeg_ = pitchDeg_;
    level_ = 0;
}
