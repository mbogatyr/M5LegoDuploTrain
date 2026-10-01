#pragma once

#include <M5Unified.h>

#include "DuploProtocol.h"
#include "TrainControls.h"
#include "TrainLink.h"

// Draws on the built-in StickS3 display (135x240).
//
// The screen is made for a small child: a frame in the colour of the train's
// LED, five chevrons up for forward and five down for backward, as many lit
// as the speed level. Outlined while the train stands (a preview of the
// tilt), filled while it runs. A dot in the corner and a word in the middle
// tell the connection state.
//
// At power-on a splash screen shows the name and a short how-to instead.
//
// The whole frame is composed in a sprite and pushed in a single call:
// drawing directly on the screen causes noticeable flicker.
class Renderer {
  public:
    // Everything the screen shows; a frame is redrawn only when it changes.
    struct Screen {
        TrainLink::State link = TrainLink::State::Searching;
        bool blink = false; // the phase of blinking elements
        int level = 0;
        bool running = false;
        duplo::Color light = duplo::Color::Off;
        const char *toast = nullptr; // a short word in the middle, or none
        bool splash = false;         // the name and the how-to instead

        bool operator==(const Screen &other) const {
            return link == other.link && blink == other.blink && level == other.level &&
                   running == other.running && light == other.light && toast == other.toast &&
                   splash == other.splash;
        }
    };

    // Call after M5.begin().
    void begin();

    // Redraws the screen only when the picture has changed.
    void draw(const Screen &screen);

    // Forgets the last frame. Needed after the display wakes up: its
    // contents are lost, and otherwise the comparison with the previous
    // frame would decide there is nothing to redraw.
    void invalidate();

    // The last frame over serial: "SNAP <w> <h>\n", then RGB565, high byte first.
    void writeSnapshot(Print &out);

  private:
    void paint(const Screen &screen);
    void paintFrame(const Screen &screen);
    void paintChevrons(const Screen &screen);
    void paintChevron(int index, int direction, uint16_t fill, bool outline, uint16_t outlineColor);
    void paintCentre(const Screen &screen);
    void paintStatus(const Screen &screen);
    void paintToast(const char *text);
    void paintSplash();

    M5Canvas canvas_{&M5.Display};

    bool hasPrevious_ = false;
    Screen previous_;
};
