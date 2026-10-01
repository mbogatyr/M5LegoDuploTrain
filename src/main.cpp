#include <M5Unified.h>
#include <Preferences.h>

#include "DisplayTimeout.h"
#include "DuploProtocol.h"
#include "Renderer.h"
#include "Repeats.h"
#include "ShakeDetector.h"
#include "TiltThrottle.h"
#include "TrainControls.h"
#include "TrainLink.h"

static_assert(TiltThrottle::kMaxLevel == TrainControls::kMaxLevel,
              "the tilt and the controls must agree on the number of levels");

namespace {

constexpr uint8_t kBrightness = 120;
constexpr uint32_t kToastMs = 1200;
constexpr uint32_t kSplashMs = 15000; // unless a key ends it sooner
constexpr uint32_t kBlinkMs = 500;

// KEY2: a second press within this time after the first one's release makes
// a double press; M5Unified also counts a press this long as a hold, which
// is no click at all.
constexpr uint32_t kKey2ClickMs = 300;
// Holding KEY2 this long makes the current grip the neutral one.
constexpr uint32_t kRecenterMs = 1500;

constexpr const char *kPrefsNamespace = "train";
constexpr const char *kPrefsNeutral = "neutral";

Renderer renderer;
DisplayTimeout displayTimeout;
TrainLink train;
TrainControls controls;
TiltThrottle tilt;
ShakeDetector shake;
// Commands that did not always take on the first try (see Repeats.h).
Repeats brakeRepeats;
Repeats lightRepeats;
Preferences prefs;

bool displayAwake = true;
bool splash = true;
TrainLink::State previousLink = TrainLink::State::Searching;

// The last motor command, and whether it still has to go out: a write that
// failed is tried again on the next tick.
int motorPower = 0;
bool motorPending = false;

const char *toast = nullptr;
uint32_t toastSinceMs = 0;

// The KEY2 press that woke the display only wakes it, but M5Unified decides
// on a click only after the release, when the display is already on.
bool swallowKey2 = false;
bool recentered = false; // during the current KEY2 hold

String command;
bool accelOn = false;
uint32_t accelSinceMs = 0;

// A made-up screen shown instead of the real one, for screenshots.
bool demoOn = false;
Renderer::Screen demoScreen;
char demoToast[16] = {};

void setDisplayAwake(bool awake) {
    if (awake == displayAwake) {
        return;
    }
    displayAwake = awake;

    if (awake) {
        M5.Display.wakeup();
        M5.Display.setBrightness(kBrightness);
        renderer.invalidate();
    } else {
        // The backlight is the main power consumer, so it is turned off
        // separately from putting the panel itself to sleep.
        M5.Display.setBrightness(0);
        M5.Display.sleep();
    }
}

void showToast(const char *text, uint32_t now) {
    toast = text;
    toastSinceMs = now;
}

// Zero power goes out as the brake: merely letting the motor go leaves the
// train rolling.
bool writeMotor(int power) {
    return train.send(power == 0 ? duplo::motorBrake() : duplo::motorPower(power));
}

void sendMotor(int power, uint32_t now) {
    motorPower = power;
    motorPending = !writeMotor(power);
    if (power == 0) {
        brakeRepeats.start(now);
    } else {
        brakeRepeats.cancel();
    }
}

// Keeps the train in line with the screen: retries a failed motor write,
// repeats the brake after a stop and the LED colour after connecting.
void maintainTrain(uint32_t now) {
    if (motorPending) {
        motorPending = !writeMotor(motorPower);
    } else if (brakeRepeats.due(now)) {
        writeMotor(0);
    }
    if (lightRepeats.due(now)) {
        train.send(duplo::ledColor(controls.lightColor()));
    }
}

void send(const TrainControls::Command &command, uint32_t now) {
    if (command.hasMotor) {
        sendMotor(command.motorPower, now);
    }
    if (command.hasSound) {
        train.send(duplo::playSound(command.sound));
        showToast(TrainControls::label(command.sound), now);
    }
    if (command.hasLight) {
        train.send(duplo::ledColor(command.light));
        lightRepeats.cancel(); // a newer colour
        showToast(TrainControls::label(command.light), now);
    }
}

// A train that has just connected stands still, and its LED is turned off,
// so that the screen and the train start out alike. One that has just gone
// is not moving any more either.
void onLinkChanged(TrainLink::State state, uint32_t now) {
    controls.reset();
    motorPower = 0;
    motorPending = false;
    brakeRepeats.cancel();
    lightRepeats.cancel();
    if (state == TrainLink::State::Connected) {
        train.send(duplo::ledColor(controls.lightColor()));
        lightRepeats.start(now);
    }
}

// The accelerometer in the axes TiltThrottle expects: X across the short
// side, Y along the long side towards the top of the screen, Z out of the
// screen.
bool readAccel(float &x, float &y, float &z) {
    float ax, ay, az;
    if (!M5.Imu.getAccel(&ax, &ay, &az)) {
        return false;
    }
    x = ax;
    y = ay;
    z = az;
    return true;
}

// Nobody may be reading the port: then the USB buffer fills up and every
// print would stall the loop, so the lines are skipped instead.
bool serialHasRoom() { return Serial.availableForWrite() >= 128; }

// "demo splash", or "demo <link> <level> <running> <colour> [toast]": link is
// s, c or k (searching, connecting, connected), running 0 or 1, colour the
// LEGO colour number (0 off .. 10 white), toast a word for the centre.
bool parseDemo(const String &line) {
    Renderer::Screen screen;
    screen.blink = true;
    if (line == "demo splash") {
        screen.splash = true;
        demoScreen = screen;
        return true;
    }

    char link = 0;
    int level = 0;
    int running = 0;
    int colour = 0;
    char toastWord[sizeof(demoToast)] = {};
    const int fields = sscanf(line.c_str(), "demo %c %d %d %d %15s", &link, &level, &running,
                              &colour, toastWord);
    if (fields < 4 || colour < 0 || colour > 10) {
        return false;
    }
    switch (link) {
    case 's':
        screen.link = TrainLink::State::Searching;
        break;
    case 'c':
        screen.link = TrainLink::State::Connecting;
        break;
    case 'k':
        screen.link = TrainLink::State::Connected;
        break;
    default:
        return false;
    }
    screen.level = level;
    screen.running = running != 0;
    screen.light = static_cast<duplo::Color>(colour);
    if (fields == 5) {
        memcpy(demoToast, toastWord, sizeof(demoToast));
        screen.toast = demoToast;
    }
    demoScreen = screen;
    return true;
}

// Line commands for self-testing: s (screenshot); toggles: acc (accelerometer
// and tilt lines at 10 Hz), ble (every message to and from the train);
// demo ... (see parseDemo) and live (back to the real screen).
void readSerial() {
    while (Serial.available() > 0) {
        const char ch = static_cast<char>(Serial.read());
        if (ch != '\n' && ch != '\r') {
            if (command.length() < 40) {
                command += ch;
            }
            continue;
        }
        if (command == "s") {
            renderer.writeSnapshot(Serial);
        } else if (command == "acc") {
            accelOn = !accelOn;
        } else if (command == "ble") {
            train.setLogging(!train.logging());
        } else if (command.startsWith("demo ")) {
            demoOn = parseDemo(command);
            Serial.println(demoOn ? "DEMO on" : "DEMO bad arguments");
        } else if (command == "live") {
            demoOn = false;
        }
        command = "";
    }
}

// awake: whether the display showed the main screen when the key went down;
// a press that wakes it or ends the splash does nothing else.
void handleKey2(bool awake, bool connected, uint32_t now) {
    m5::Button_Class &key = M5.BtnB;

    if (key.wasPressed() && !awake) {
        swallowKey2 = true;
    }

    if (key.wasDecideClickCount()) {
        if (!swallowKey2 && connected) {
            if (key.getClickCount() == 1) {
                send(controls.nextSound(), now);
            } else {
                send(controls.nextLight(), now);
            }
        }
        swallowKey2 = false;
    }

    if (key.pressedFor(kRecenterMs) && !recentered) {
        recentered = true;
        if (!swallowKey2) {
            tilt.recenter();
            prefs.putFloat(kPrefsNeutral, tilt.neutralDeg());
            showToast("Zero set", now);
        }
    }
    if (key.wasReleasedAfterHold()) {
        recentered = false;
        swallowKey2 = false;
    }
}

} // namespace

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    // M5Unified leaves Serial alone (its serial_baudrate is 0 by default).
    Serial.begin(115200);

    M5.BtnB.setHoldThresh(kKey2ClickMs);

    prefs.begin(kPrefsNamespace, false);
    tilt = TiltThrottle(prefs.getFloat(kPrefsNeutral, TiltThrottle::kDefaultNeutralDeg));

    M5.Display.setBrightness(kBrightness);
    renderer.begin();
    displayTimeout.begin(millis());
    train.begin();
}

void loop() {
    M5.update();
    readSerial();

    // First, since connecting blocks for a while: the time is taken after it.
    train.update();

    const uint32_t now = millis();

    if (train.state() != previousLink) {
        onLinkChanged(train.state(), now);
        previousLink = train.state();
    }
    const bool connected = train.state() == TrainLink::State::Connected;

    float ax = 0, ay = 0, az = 0;
    bool shook = false;
    if (readAccel(ax, ay, az)) {
        tilt.update(ax, ay, az);
        shook = shake.update(now, ax, ay, az);
    }
    if (accelOn && now - accelSinceMs >= 100 && serialHasRoom()) {
        Serial.printf("ACC %.3f %.3f %.3f pitch=%.1f neutral=%.1f level=%d\n", ax, ay, az,
                      tilt.pitchDeg(), tilt.neutralDeg(), tilt.level());
        accelSinceMs = now;
    }

    if (connected) {
        maintainTrain(now);
    }

    const int levelBefore = controls.level();
    send(controls.setLevel(tilt.level()), now);
    if (shook && connected) {
        send(controls.horn(), now);
    }

    // A running train keeps the display on: with the screen dark, the first
    // press of KEY1 would only wake it instead of stopping the train.
    const bool wasAwake = displayAwake;
    const bool activity = M5.BtnA.isPressed() || M5.BtnB.isPressed() || controls.running() ||
                          controls.level() != levelBefore || shook;
    setDisplayAwake(displayTimeout.shouldBeOn(now, activity));

    // A press that wakes the display or ends the splash does nothing else.
    const bool keyDown = M5.BtnA.wasPressed() || M5.BtnB.wasPressed();
    const bool ready = wasAwake && !splash;
    if (splash && (keyDown || now >= kSplashMs)) {
        splash = false;
    }
    if (ready && M5.BtnA.wasPressed() && connected) {
        send(controls.toggleRun(), now);
    }
    handleKey2(ready, connected, now);

    if (toast != nullptr && now - toastSinceMs >= kToastMs) {
        toast = nullptr;
    }

    if (displayAwake) {
        Renderer::Screen screen;
        screen.link = train.state();
        screen.blink = (now / kBlinkMs) % 2 == 0;
        screen.level = controls.level();
        screen.running = controls.running();
        screen.light = controls.lightColor();
        screen.toast = toast;
        screen.splash = splash;
        renderer.draw(demoOn ? demoScreen : screen);
    }

    delay(20);
}
