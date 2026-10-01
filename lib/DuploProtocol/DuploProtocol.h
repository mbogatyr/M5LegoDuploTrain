#pragma once

#include <stddef.h>
#include <stdint.h>

// The subset of the LEGO Wireless Protocol 3.0 that the DUPLO Train Base
// (the locomotive of set 10875) needs: building commands and recognising
// what the hub sends back.
//
// Like everything in lib/, it does not touch Bluetooth: messages are plain
// byte arrays, TrainLink in src/ writes them to the characteristic.
namespace duplo {

// The hub exposes a single characteristic: commands are written to it, and
// the hub's messages arrive as notifications from it.
constexpr const char *kServiceUuid = "00001623-1212-efde-1623-785feabcd123";
constexpr const char *kCharacteristicUuid = "00001624-1212-efde-1623-785feabcd123";

// LEGO's Bluetooth company id, as it comes first in the manufacturer data
// (little-endian 0x0397).
constexpr uint8_t kLegoCompanyIdLow = 0x97;
constexpr uint8_t kLegoCompanyIdHigh = 0x03;

// System type and device number in the advertisement: DUPLO Train Base.
constexpr uint8_t kSystemTypeTrain = 0x20;

// The hub's built-in ports.
constexpr uint8_t kPortMotor = 0x00;
constexpr uint8_t kPortSpeaker = 0x01;
constexpr uint8_t kPortLed = 0x11;

constexpr int kMaxPower = 100;

// The sounds the locomotive has built in, with their numbers on the wire.
enum class Sound : uint8_t {
    Brake = 3,
    StationDeparture = 5,
    WaterRefill = 7,
    Horn = 9,
    Steam = 10,
};

// The LED palette of LEGO hubs.
enum class Color : uint8_t {
    Off = 0,
    Pink = 1,
    Purple = 2,
    Blue = 3,
    LightBlue = 4,
    Cyan = 5,
    Green = 6,
    Yellow = 7,
    Orange = 8,
    Red = 9,
    White = 10,
};

// A complete message, starting with the common header (length, hub id).
struct Message {
    static constexpr size_t kCapacity = 10;

    uint8_t bytes[kCapacity] = {};
    uint8_t length = 0;
};

// Motor power in percent, from -kMaxPower (full reverse) to kMaxPower.
// Values outside the range are clamped. 0 only lets the motor go: the train
// rolls on, and may take that for a push and drive off by itself.
Message motorPower(int percent);

// Brakes the motor: stops the train at once.
Message motorBrake();

Message playSound(Sound sound);

Message ledColor(Color color);

// Switch the speaker to its "sound" mode and the LED to its "colour index"
// mode. Sent once after connecting, before playSound() and ledColor().
Message speakerSoundMode();
Message ledColorMode();

// There is no battery message: the DUPLO Train Base answers neither the
// battery hub property (0x06) nor a subscription to its voltage port (0x14).

// manufacturerData starts with the company id, the way NimBLE returns it.
bool isTrainAdvertisement(const uint8_t *manufacturerData, size_t length);

} // namespace duplo
