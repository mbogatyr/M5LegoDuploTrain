#pragma once

#include <NimBLEDevice.h>

#include <atomic>

#include "DuploProtocol.h"

// The Bluetooth LE connection to the DUPLO train.
//
// Scans until it sees a DUPLO Train Base, connects to the first one, and
// goes back to scanning when the connection is lost (the train was switched
// off or went out of range).
//
// NimBLE calls back from its own task, so the callbacks only set atomics;
// connecting and writing happen in update() and send(), from loop().
class TrainLink : NimBLEScanCallbacks, NimBLEClientCallbacks {
  public:
    enum class State : uint8_t {
        Searching,
        Connecting,
        Connected,
    };

    // Call after M5.begin().
    void begin();

    // Call every loop() tick. When a train has been found, the first call
    // only switches to Connecting, so that a frame saying so can be drawn;
    // the next one connects, which blocks for up to a few seconds.
    void update();

    // Returns false when not connected or the write failed.
    bool send(const duplo::Message &message);

    State state() const { return state_; }

    // Prints every message to and from the train on Serial, in hex.
    void setLogging(bool on);
    bool logging() const;

  private:
    void onResult(const NimBLEAdvertisedDevice *device) override;
    void onDisconnect(NimBLEClient *client, int reason) override;

    void startSearching();
    bool connect();

    NimBLEClient *client_ = nullptr;
    NimBLERemoteCharacteristic *characteristic_ = nullptr;

    State state_ = State::Searching;

    // Written by the scan callback before found_ is set, read by update()
    // after it sees found_.
    NimBLEAddress trainAddress_;
    std::atomic<bool> found_{false};
    std::atomic<bool> disconnected_{false};
};
