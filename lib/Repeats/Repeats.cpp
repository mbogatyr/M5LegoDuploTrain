#include "Repeats.h"

void Repeats::start(uint32_t nowMs) {
    startMs_ = nowMs;
    next_ = 0;
}

bool Repeats::due(uint32_t nowMs) {
    if (next_ >= kCount) {
        return false;
    }
    // Signed, so that a moment slightly before start() counts as not yet
    // rather than as 49 days later; the subtraction itself still handles the
    // millis() rollover.
    const int32_t elapsed = static_cast<int32_t>(nowMs - startMs_);
    if (elapsed < static_cast<int32_t>(kAtMs[next_])) {
        return false;
    }
    // After a long stall (a blocking reconnect), the missed repeats collapse
    // into this one.
    while (next_ < kCount && elapsed >= static_cast<int32_t>(kAtMs[next_])) {
        ++next_;
    }
    return true;
}
