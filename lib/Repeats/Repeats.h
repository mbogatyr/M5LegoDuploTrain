#pragma once

#include <stdint.h>

// Says when to send a command to the train again.
//
// Some commands did not always take, with nothing in the log to tell why:
// a stop after which the train drove on, the LED colour right after
// connecting. Such a command goes out a few more times while the train
// settles, then the repeats end, so that an idle train can still go to sleep
// by itself.
//
// Like everything in lib/, it never touches hardware: the time goes in, a
// yes or no comes out.
class Repeats {
  public:
    // When to repeat, counted from start().
    static constexpr int kCount = 4;
    static constexpr uint32_t kAtMs[kCount] = {200, 600, 1500, 3000};

    // The command has just gone out: repeat it from now on, starting over if
    // repeats were already running.
    void start(uint32_t nowMs);

    // Nothing more to repeat: a newer command replaced it, or the train is
    // gone.
    void cancel() { next_ = kCount; }

    // True when the command is due again; each repeat is reported once.
    bool due(uint32_t nowMs);

  private:
    uint32_t startMs_ = 0;
    int next_ = kCount; // the index of the next repeat; kCount when none
};
