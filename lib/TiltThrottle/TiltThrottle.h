#pragma once

// Turns the tilt of the stick into a speed level.
//
// The stick is held upright in portrait, screen towards the driver. Tipping
// the top edge away from the driver means forward, towards the driver means
// backward. Angles are counted from a neutral pose: a dead zone around it
// means standing, every further kStepDeg adds a level, up to kMaxLevel.
//
// Board axes: X across the short side, Y along the long side towards the top
// of the screen, Z out of the screen. main.cpp maps M5Unified's readings to
// them.
//
// Like everything in lib/, it never touches hardware: acceleration in g goes
// in, a level comes out.
class TiltThrottle {
  public:
    static constexpr int kMaxLevel = 5;
    static constexpr float kDeadZoneDeg = 6.0f; // either side of neutral
    static constexpr float kStepDeg = 6.0f;     // per level beyond the dead zone
    // How far past a level boundary the tilt has to go before the level
    // changes, in levels: keeps a shaky hand from flickering between two.
    static constexpr float kHysteresis = 0.25f;
    static constexpr float kSmoothing = 0.3f; // share of a new reading in the filter

    // Upright is 0, lying flat with the screen up is 90. The grip for
    // looking at the screen while driving measured 50-62 on 2026-10-01.
    static constexpr float kDefaultNeutralDeg = 55.0f;

    explicit TiltThrottle(float neutralDeg = kDefaultNeutralDeg) : neutralDeg_(neutralDeg) {}

    // Takes an accelerometer reading. Readings far from 1 g (a shake, a
    // bump, free fall) are ignored, so they do not move the level.
    void update(float ax, float ay, float az);

    // -kMaxLevel..kMaxLevel, above 0 forward.
    int level() const { return level_; }

    // The filtered tilt from upright, towards the screen facing up.
    float pitchDeg() const { return pitchDeg_; }

    // The current pose becomes neutral; the level goes to 0.
    void recenter();

    float neutralDeg() const { return neutralDeg_; }

  private:
    static float wrap180(float deg);
    static float positionFor(float deg); // signed, in levels; |x| < 1 is the dead zone

    float neutralDeg_;
    float gy_ = 0;
    float gz_ = 0;
    bool hasReading_ = false;
    float pitchDeg_ = 0;
    int level_ = 0;
};
