// Effects are evaluated at the real LED positions in metres: x along a row, y across the rows.
// Detail across the rows is kept soft, and edges are widened by each row's footprint (half the gap
// to its neighbours) so they don't pop as they cross a row. Stripe patterns are attenuated by the
// same footprint (area averaging).
#pragma once
#include <FastLED.h>
#include "layout.h"

constexpr int MAXP = 8;

struct ParamDef {
    const char* name;
    float min, max, def, step;
};

struct Ctx {
    const float* p;       // parameters, see the effect's ParamDef list
    float dt;             // seconds since the last frame
    float* s;             // per-effect state (MAXP floats), zeroed when the preset changes
    const CRGB* lutWrap;  // palette as a loop
    const CRGB* lutLin;   // palette as a ramp, first colour -> last colour
    CRGB* out;            // [row * nx + led]
};

struct EffectDef {
    const char* name;
    uint8_t n;
    ParamDef params[MAXP];
    void (*render)(Ctx&);
    bool ownSparkle;      // the effect is sparkle itself; no overlay on top
};

extern const EffectDef EFFECTS[];
extern const int NUM_EFFECTS;

// (Re)compute LED positions from the layout; call after a layout change
void effectsInit();
// White twinkles added on top of any effect. density 0..1.
void sparkleOverlay(CRGB* out, float density, float dt);
