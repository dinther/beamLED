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
    const char* help;  // what it does, for the page's tooltip and OSCQuery's description
};

struct Ctx {
    const float* p;       // parameters, see the effect's ParamDef list
    float dt;             // seconds since the last frame
    float* s;             // per-effect state (MAXP floats), zeroed when the preset changes
    const CRGB* lutWrap;  // palette as a loop
    const CRGB* lutLin;   // palette as a ramp, first colour -> last colour
    CRGB* out;            // [row * nx + led]
    // Beat sync (Ableton Link). dt is already scaled by the tempo when the preset follows it.
    double beat;          // beats since the session origin (bar = 4 beats)
    float bpm;
    float cycleBeats;     // > 0: timing in beats, the effect's cycle takes this many beats
    bool reverse;         // timing in beats: run the cycle backwards
};

struct EffectDef {
    const char* name;
    uint8_t n;
    ParamDef params[MAXP];
    void (*render)(Ctx&);
    bool ownSparkle;      // the effect is sparkle itself; no overlay on top
    // Timing in beats: what one cycle is ("band", "revolution", ...) and which parameter it
    // replaces (the speed). nullptr / -1: the effect has no cycle, time timing only.
    const char* cycle;
    int8_t speedParam;
    const char* help;  // what the effect looks like
};

extern const EffectDef EFFECTS[];
extern const int NUM_EFFECTS;

// (Re)compute LED positions from the layout; call after a layout change
void effectsInit();
// ms per frame for effect e at its defaults (scratch buffer; the effect playing isn't disturbed)
float effectsBenchmark(int e, const CRGB* lutWrap, const CRGB* lutLin, int frames);
// Twinkles added on top of any effect. density 0..1. lut: their colours (a palette as a loop,
// each twinkle a random place on it); nullptr = white.
void sparkleOverlay(CRGB* out, float density, float dt, const CRGB* lut = nullptr);
