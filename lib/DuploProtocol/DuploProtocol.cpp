#include "DuploProtocol.h"

#include <initializer_list>

namespace duplo {

namespace {

constexpr uint8_t kHubId = 0x00;

// Message types
constexpr uint8_t kPortInputFormatSetup = 0x41;
constexpr uint8_t kPortOutputCommand = 0x81;

// Port output: execute immediately and report when done
constexpr uint8_t kStartupAndCompletion = 0x11;
constexpr uint8_t kWriteDirectModeData = 0x51;

constexpr uint8_t kMotorModePower = 0x00;
constexpr uint8_t kMotorBrake = 0x7F; // a power value of its own in LWP3
constexpr uint8_t kSpeakerModeSound = 0x01;
constexpr uint8_t kLedModeColor = 0x00;

// Prepends the common header: the total length and the hub id.
Message withHeader(std::initializer_list<uint8_t> body) {
    Message message;
    message.bytes[message.length++] = static_cast<uint8_t>(body.size() + 2);
    message.bytes[message.length++] = kHubId;
    for (uint8_t byte : body) {
        message.bytes[message.length++] = byte;
    }
    return message;
}

Message writeDirect(uint8_t port, uint8_t mode, uint8_t value) {
    return withHeader({kPortOutputCommand, port, kStartupAndCompletion,
                       kWriteDirectModeData, mode, value});
}

// The delta of 1 and the notification flag are what Legoino sends; the
// delta is irrelevant for output-only modes.
Message inputFormat(uint8_t port, uint8_t mode, bool notify) {
    return withHeader({kPortInputFormatSetup, port, mode, 0x01, 0x00, 0x00,
                       0x00, static_cast<uint8_t>(notify ? 0x01 : 0x00)});
}

} // namespace

Message motorPower(int percent) {
    if (percent > kMaxPower) {
        percent = kMaxPower;
    } else if (percent < -kMaxPower) {
        percent = -kMaxPower;
    }
    // The power travels as a signed byte: -50 is 0xCE.
    return writeDirect(kPortMotor, kMotorModePower,
                       static_cast<uint8_t>(static_cast<int8_t>(percent)));
}

Message motorBrake() { return writeDirect(kPortMotor, kMotorModePower, kMotorBrake); }

Message playSound(Sound sound) {
    return writeDirect(kPortSpeaker, kSpeakerModeSound, static_cast<uint8_t>(sound));
}

Message ledColor(Color color) {
    return writeDirect(kPortLed, kLedModeColor, static_cast<uint8_t>(color));
}

Message speakerSoundMode() { return inputFormat(kPortSpeaker, kSpeakerModeSound, true); }

Message ledColorMode() { return inputFormat(kPortLed, kLedModeColor, false); }

bool isTrainAdvertisement(const uint8_t *manufacturerData, size_t length) {
    // Company id (2 bytes), button state, system type and device number
    return length >= 4 && manufacturerData[0] == kLegoCompanyIdLow &&
           manufacturerData[1] == kLegoCompanyIdHigh &&
           manufacturerData[3] == kSystemTypeTrain;
}

} // namespace duplo
