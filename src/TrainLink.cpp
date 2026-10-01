#include "TrainLink.h"

#include <Arduino.h>

namespace {

constexpr uint32_t kConnectTimeoutMs = 5000;

// After subscribing, the hub announces its ports; Legoino waits this long
// before configuring them.
constexpr uint32_t kSettleMs = 200;

// Between the setup messages, and after the last one, so that the first
// queued message keeps the gap too. Sent back to back with the first LED
// colour, they were not all handled: one was acknowledged, one answered with
// an error, and the colour was lost.
constexpr uint32_t kSetupGapMs = CommandQueue::kGapMs;

// Read from the NimBLE task too, for messages from the train.
std::atomic<bool> loggingOn{false};

void logBytes(const char *direction, const uint8_t *data, size_t length) {
    // Nobody may be reading the port: then a print would stall.
    if (!loggingOn.load() || Serial.availableForWrite() < 128) {
        return;
    }
    char hex[3 * 32 + 1] = {};
    for (size_t i = 0; i < length && i < 32; ++i) {
        snprintf(hex + 3 * i, 4, "%02X ", data[i]);
    }
    Serial.printf("%8lu %s %s\n", static_cast<unsigned long>(millis()), direction, hex);
}

bool write(NimBLERemoteCharacteristic *characteristic, const duplo::Message &message) {
    logBytes("->", message.bytes, message.length);
    const bool written = characteristic->writeValue(message.bytes, message.length, false);
    if (!written && Serial.availableForWrite() >= 128) {
        Serial.println("Write failed");
    }
    return written;
}

} // namespace

void TrainLink::begin() {
    NimBLEDevice::init("");

    client_ = NimBLEDevice::createClient();
    client_->setClientCallbacks(this, false);
    client_->setConnectTimeout(kConnectTimeoutMs);

    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(this, false);
    // Active scanning asks for the scan response too, so onResult() sees
    // the hub's complete advertisement.
    scan->setActiveScan(true);

    startSearching();
}

void TrainLink::update() {
    switch (state_) {
    case State::Searching:
        if (found_.load(std::memory_order_acquire)) {
            state_ = State::Connecting;
        }
        break;

    case State::Connecting:
        if (connect()) {
            state_ = State::Connected;
            Serial.println("Train connected");
        } else {
            Serial.println("Connection failed");
            client_->disconnect();
            startSearching();
        }
        break;

    case State::Connected: {
        if (disconnected_.load() || !client_->isConnected()) {
            Serial.println("Train disconnected");
            startSearching();
            break;
        }
        duplo::Message message;
        if (queue_.next(millis(), message) && !write(characteristic_, message)) {
            queue_.retry(message);
        }
        break;
    }
    }
}

bool TrainLink::send(const duplo::Message &message) {
    if (state_ != State::Connected || characteristic_ == nullptr) {
        return false;
    }
    queue_.put(message);
    return true;
}

void TrainLink::setLogging(bool on) { loggingOn.store(on); }

bool TrainLink::logging() const { return loggingOn.load(); }

void TrainLink::onResult(const NimBLEAdvertisedDevice *device) {
    if (found_.load()) {
        return;
    }

    const std::string data = device->getManufacturerData();
    const auto *bytes = reinterpret_cast<const uint8_t *>(data.data());
    if (!duplo::isTrainAdvertisement(bytes, data.size())) {
        return;
    }

    Serial.printf("Found a DUPLO train: %s\n", device->getAddress().toString().c_str());
    NimBLEDevice::getScan()->stop();
    trainAddress_ = device->getAddress();
    found_.store(true, std::memory_order_release);
}

void TrainLink::onDisconnect(NimBLEClient *, int reason) {
    Serial.printf("Disconnected, reason %d\n", reason);
    disconnected_.store(true);
}

void TrainLink::startSearching() {
    state_ = State::Searching;
    characteristic_ = nullptr;
    queue_.clear();
    found_.store(false);

    // 0 scans until stopped; the restart flag clears the duplicate filter,
    // so a train seen before is reported again.
    NimBLEDevice::getScan()->start(0, false, true);
    Serial.println("Searching for the train");
}

bool TrainLink::connect() {
    disconnected_.store(false);

    if (!client_->connect(trainAddress_)) {
        return false;
    }

    NimBLERemoteService *service = client_->getService(duplo::kServiceUuid);
    if (service == nullptr) {
        return false;
    }
    NimBLERemoteCharacteristic *characteristic =
        service->getCharacteristic(duplo::kCharacteristicUuid);
    if (characteristic == nullptr) {
        return false;
    }

    // Nothing the train sends is needed to drive it (port feedback, errors,
    // the list of its ports), but it is worth seeing in the log.
    const bool subscribed = characteristic->subscribe(
        true, [](NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
            logBytes("<-", data, length);
        });
    if (!subscribed) {
        return false;
    }

    delay(kSettleMs);

    const duplo::Message setup[] = {
        duplo::speakerSoundMode(),
        duplo::ledColorMode(),
    };
    for (const duplo::Message &message : setup) {
        if (!write(characteristic, message)) {
            return false;
        }
        delay(kSetupGapMs);
    }

    characteristic_ = characteristic;
    return true;
}
