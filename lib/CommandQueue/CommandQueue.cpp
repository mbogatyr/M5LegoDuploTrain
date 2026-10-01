#include "CommandQueue.h"

namespace {

constexpr uint8_t kPortOutputCommand = 0x81;

} // namespace

int CommandQueue::slotFor(const duplo::Message &message) {
    // Length, hub id, message type, port, ...
    if (message.length < 4 || message.bytes[2] != kPortOutputCommand) {
        return kOther;
    }
    switch (message.bytes[3]) {
    case duplo::kPortMotor:
        return kMotor;
    case duplo::kPortSpeaker:
        return kSpeaker;
    case duplo::kPortLed:
        return kLed;
    default:
        return kOther;
    }
}

void CommandQueue::put(const duplo::Message &message) {
    const int slot = slotFor(message);
    waiting_[slot] = message;
    isWaiting_[slot] = true;
}

bool CommandQueue::next(uint32_t nowMs, duplo::Message &message) {
    // Signed, so that a time taken before the last hand-out counts as too
    // early; the subtraction itself handles the millis() rollover.
    if (hasSent_ && static_cast<int32_t>(nowMs - lastMs_) < static_cast<int32_t>(kGapMs)) {
        return false;
    }
    for (int slot = 0; slot < kSlotCount; ++slot) {
        if (isWaiting_[slot]) {
            message = waiting_[slot];
            isWaiting_[slot] = false;
            hasSent_ = true;
            lastMs_ = nowMs;
            return true;
        }
    }
    return false;
}

void CommandQueue::retry(const duplo::Message &message) {
    const int slot = slotFor(message);
    if (!isWaiting_[slot]) {
        waiting_[slot] = message;
        isWaiting_[slot] = true;
    }
}

void CommandQueue::clear() {
    for (bool &waiting : isWaiting_) {
        waiting = false;
    }
}

bool CommandQueue::empty() const {
    for (bool waiting : isWaiting_) {
        if (waiting) {
            return false;
        }
    }
    return true;
}
