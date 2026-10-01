#pragma once

#include <stdint.h>

#include "DuploProtocol.h"

// Paces the messages to the train: one at a time, at least kGapMs apart.
//
// The train does not take two commands written back to back: of a brake and
// a brake sound sent within the same millisecond, it acknowledged only one,
// and the other was simply lost (the brake in four stops out of five, the
// sound in others). Commands 44 ms apart were both acknowledged, and so were
// setup messages 100 ms apart.
//
// Only the latest message per port waits: a newer motor power replaces one
// that has not gone out yet, and so on for the speaker and the LED. When
// several ports wait, the motor goes first, then the speaker, then the LED.
//
// Like everything in lib/, it never touches hardware: messages and the time
// go in, the message to write comes out.
class CommandQueue {
  public:
    static constexpr uint32_t kGapMs = 100;

    // Replaces the message waiting for the same port, if any.
    void put(const duplo::Message &message);

    // The message to write now: true when one is waiting and kGapMs have
    // passed since the last one handed out.
    bool next(uint32_t nowMs, duplo::Message &message);

    // Puts back a message whose write failed, unless a newer one for its
    // port is already waiting. It goes out after the usual gap.
    void retry(const duplo::Message &message);

    // Drops everything waiting: the train is gone.
    void clear();

    bool empty() const;

  private:
    // In the order they go out.
    enum Slot { kMotor, kSpeaker, kLed, kOther, kSlotCount };

    static int slotFor(const duplo::Message &message);

    duplo::Message waiting_[kSlotCount];
    bool isWaiting_[kSlotCount] = {};
    bool hasSent_ = false;
    uint32_t lastMs_ = 0;
};
