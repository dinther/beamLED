// Effects are evaluated at the 2700 real LED positions in metres: x along a bar, y across
// the bars (measured). Detail across the bars is kept >= ~0.8 m, and sharp edges are widened
// by each bar's footprint (half the gap to its neighbours) so they don't pop as they cross a bar.
#pragma once
#include <FastLED.h>
#include "config.h"

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
    CRGB* out;            // [bar * PIX + pixel]
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

void effectsInit();
// White twinkles added on top of any effect. density 0..1.
void sparkleOverlay(CRGB* out, float density, float dt);
