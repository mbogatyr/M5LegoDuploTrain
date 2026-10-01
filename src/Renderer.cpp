#include "Renderer.h"

namespace {

using duplo::Color;
using State = TrainLink::State;

// The frame: an outer rounded rectangle in the LED colour, an inner black
// one cut out of it. Everything else stays inside the inner one.
constexpr int kFrameInset = 2;
constexpr int kFrameWidth = 4;
constexpr int kFrameRadius = 12;

// The chevrons, in pixels. Forward ones stack up from the centre, backward
// ones are their mirror image below it. The top of the farthest one is
// kCentreGap + 4 * kChevronPitch + kChevronThickness + kChevronRise = 102
// pixels from the centre, which leaves 12 pixels to the inner edge of the
// frame above and 11 below.
constexpr int kCentreY = 120;
constexpr int kCentreGap = 12; // from the centre to the nearest chevron
constexpr int kChevronHalfWidth = 39;
constexpr int kChevronRise = 14;      // how much higher the middle is than the ends
constexpr int kChevronThickness = 8;  // vertical
constexpr int kChevronPitch = 17;     // from one chevron to the next

constexpr int kDotRadius = 7; // the centre dot while connected

constexpr uint16_t kUnlit = 0x18E3;    // the chevrons beyond the level
constexpr uint16_t kDimFrame = 0x39E7; // the frame while the LED is off
constexpr uint16_t kForward = 0x3EAD;  // a bright green
constexpr uint16_t kBackward = 0xFCC5; // an orange
constexpr uint16_t kToastBack = 0x3186;

// The screen's rendition of the train's LED colours.
uint16_t ledRgb(Color color) {
    switch (color) {
    case Color::Off:
        return kDimFrame;
    case Color::Pink:
        return lgfx::color565(255, 110, 180);
    case Color::Purple:
        return lgfx::color565(150, 60, 220);
    case Color::Blue:
        return lgfx::color565(30, 70, 255);
    case Color::LightBlue:
        return lgfx::color565(110, 170, 255);
    case Color::Cyan:
        return TFT_CYAN;
    case Color::Green:
        return TFT_GREEN;
    case Color::Yellow:
        return TFT_YELLOW;
    case Color::Orange:
        return TFT_ORANGE;
    case Color::Red:
        return TFT_RED;
    case Color::White:
        return TFT_WHITE;
    }
    return kDimFrame;
}

} // namespace

void Renderer::begin() {
    M5.Display.setRotation(0); // portrait: 135 wide, 240 tall
    M5.Display.fillScreen(TFT_BLACK);

    canvas_.setColorDepth(16);
    canvas_.setPsram(true); // 135*240*2 = 65 KB; the StickS3 has 8 MB of PSRAM
    canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void Renderer::invalidate() { hasPrevious_ = false; }

void Renderer::draw(const Screen &screen) {
    if (hasPrevious_ && previous_ == screen) {
        return;
    }

    paint(screen);

    hasPrevious_ = true;
    previous_ = screen;
}

void Renderer::writeSnapshot(Print &out) {
    out.printf("SNAP %d %d\n", canvas_.width(), canvas_.height());
    // A 16-bit LovyanGFX sprite already keeps its pixels high byte first,
    // so the buffer goes out as it is.
    out.write(static_cast<const uint8_t *>(canvas_.getBuffer()),
              canvas_.width() * canvas_.height() * 2);
    out.flush();
}

void Renderer::paint(const Screen &screen) {
    canvas_.fillSprite(TFT_BLACK);

    paintFrame(screen);
    if (screen.splash) {
        paintSplash();
    } else {
        paintChevrons(screen);
        paintCentre(screen);
        if (screen.toast != nullptr) {
            paintToast(screen.toast);
        }
    }
    paintStatus(screen); // the search goes on behind the splash

    canvas_.pushSprite(0, 0);
}

void Renderer::paintFrame(const Screen &screen) {
    const int w = canvas_.width();
    const int h = canvas_.height();
    canvas_.fillRoundRect(kFrameInset, kFrameInset, w - 2 * kFrameInset, h - 2 * kFrameInset,
                          kFrameRadius, ledRgb(screen.light));
    const int inner = kFrameInset + kFrameWidth;
    canvas_.fillRoundRect(inner, inner, w - 2 * inner, h - 2 * inner, kFrameRadius - kFrameWidth,
                          TFT_BLACK);
}

void Renderer::paintChevrons(const Screen &screen) {
    const bool connected = screen.link == State::Connected;
    const int lit = screen.level > 0 ? screen.level : -screen.level;
    const int litDirection = screen.level > 0 ? 1 : -1;
    const uint16_t litColor = screen.level > 0 ? kForward : kBackward;

    for (int direction = -1; direction <= 1; direction += 2) {
        for (int i = 0; i < TrainControls::kMaxLevel; ++i) {
            const bool isLit = direction == litDirection && i < lit;
            if (!isLit) {
                paintChevron(i, direction, kUnlit, false, 0);
            } else if (connected && screen.running) {
                paintChevron(i, direction, litColor, false, 0);
            } else {
                // A preview: what KEY1 would start. Grey without a train.
                paintChevron(i, direction, TFT_BLACK, true, connected ? TFT_WHITE : TFT_DARKGREY);
            }
        }
    }
}

// index 0 is the one nearest the centre; direction 1 is forward (up).
void Renderer::paintChevron(int index, int direction, uint16_t fill, bool outline,
                            uint16_t outlineColor) {
    const int cx = canvas_.width() / 2;
    // Distances from the centre, upwards: the bottom of the ends, the top
    // of the ends, the top of the middle and the bottom of the middle.
    const int endsBottom = kCentreGap + index * kChevronPitch;
    const int endsTop = endsBottom + kChevronThickness;
    const int middleTop = endsTop + kChevronRise;
    const int middleBottom = middleTop - kChevronThickness;
    auto y = [direction](int distance) { return kCentreY - direction * distance; };

    const int left = cx - kChevronHalfWidth;
    const int right = cx + kChevronHalfWidth;

    // The outline, clockwise from the left end's top: left top, middle top,
    // right top, right bottom, middle bottom, left bottom.
    const int xs[6] = {left, cx, right, right, cx, left};
    const int ys[6] = {y(endsTop), y(middleTop), y(endsTop), y(endsBottom), y(middleBottom),
                       y(endsBottom)};

    // Two arms, each a quadrilateral split into two triangles.
    canvas_.fillTriangle(xs[0], ys[0], xs[1], ys[1], xs[4], ys[4], fill);
    canvas_.fillTriangle(xs[0], ys[0], xs[4], ys[4], xs[5], ys[5], fill);
    canvas_.fillTriangle(xs[1], ys[1], xs[2], ys[2], xs[3], ys[3], fill);
    canvas_.fillTriangle(xs[1], ys[1], xs[3], ys[3], xs[4], ys[4], fill);

    if (outline) {
        for (int i = 0; i < 6; ++i) {
            const int j = (i + 1) % 6;
            canvas_.drawWideLine(xs[i], ys[i], xs[j], ys[j], 1.0f, outlineColor);
        }
    }
}

void Renderer::paintCentre(const Screen &screen) {
    const int cx = canvas_.width() / 2;

    if (screen.link == State::Connected) {
        // Filled while the train runs, a ring while it stands.
        if (screen.running) {
            canvas_.fillCircle(cx, kCentreY, kDotRadius, TFT_WHITE);
        } else {
            canvas_.fillCircle(cx, kCentreY, kDotRadius, TFT_WHITE);
            canvas_.fillCircle(cx, kCentreY, kDotRadius - 2, TFT_BLACK);
        }
        return;
    }

    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);
    if (screen.link == State::Connecting) {
        canvas_.setTextColor(TFT_YELLOW);
        canvas_.drawString("Connecting...", cx, kCentreY);
    } else {
        canvas_.setTextColor(TFT_ORANGE);
        canvas_.drawString("Searching...", cx, kCentreY);
    }
}

// A dot in the top left corner: green when connected, yellow while
// connecting, blinking orange while searching.
void Renderer::paintStatus(const Screen &screen) {
    constexpr int kStatusX = 17;
    constexpr int kStatusY = 17;
    constexpr int kStatusRadius = 4;
    switch (screen.link) {
    case State::Connected:
        canvas_.fillCircle(kStatusX, kStatusY, kStatusRadius, TFT_GREEN);
        break;
    case State::Connecting:
        canvas_.fillCircle(kStatusX, kStatusY, kStatusRadius, TFT_YELLOW);
        break;
    case State::Searching:
        if (screen.blink) {
            canvas_.fillCircle(kStatusX, kStatusY, kStatusRadius, TFT_ORANGE);
        } else {
            canvas_.drawCircle(kStatusX, kStatusY, kStatusRadius, TFT_ORANGE);
        }
        break;
    }
}

void Renderer::paintToast(const char *text) {
    constexpr int kWidth = 95;
    constexpr int kHeight = 26;
    const int cx = canvas_.width() / 2;
    canvas_.fillRoundRect(cx - kWidth / 2, kCentreY - kHeight / 2, kWidth, kHeight, 8, kToastBack);
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(TFT_WHITE);
    canvas_.drawString(text, cx, kCentreY);
}

// The name, then one line per control: the key on the left, what it does on
// the right. In Font2 the longest line, "Hold KEY2" and "zero", is about 90
// pixels wide, which leaves room between the two within the frame.
void Renderer::paintSplash() {
    const int cx = canvas_.width() / 2;
    constexpr int kLeft = 14;
    const int right = canvas_.width() - 14;

    canvas_.setTextDatum(middle_center);
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextColor(TFT_YELLOW);
    canvas_.drawString("Lego Duplo", cx, 20); // clear of "Train R/C" below
    canvas_.setFont(&fonts::Font4);
    canvas_.setTextColor(TFT_WHITE);
    canvas_.drawString("Train R/C", cx, 52);

    canvas_.drawFastHLine(kLeft, 72, right - kLeft, TFT_DARKGREY);

    struct Line {
        const char *key;
        const char *action;
    };
    constexpr Line kLines[] = {
        {"Tilt", "speed"},  {"KEY1", "go/stop"},   {"KEY2", "sound"},
        {"KEY2 x2", "light"}, {"Hold KEY2", "zero"}, {"Shake", "horn"},
    };
    constexpr int kFirstY = 88;
    constexpr int kLineHeight = 21;

    canvas_.setFont(&fonts::Font2);
    int y = kFirstY;
    for (const Line &line : kLines) {
        canvas_.setTextDatum(middle_left);
        canvas_.setTextColor(TFT_YELLOW);
        canvas_.drawString(line.key, kLeft, y);
        canvas_.setTextDatum(middle_right);
        canvas_.setTextColor(TFT_WHITE);
        canvas_.drawString(line.action, right, y);
        y += kLineHeight;
    }

    canvas_.drawFastHLine(kLeft, 210, right - kLeft, TFT_DARKGREY);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(TFT_DARKGREY);
    canvas_.drawString("Press a key", cx, 222);
}
