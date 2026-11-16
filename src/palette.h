#pragma once
#include <FastLED.h>

constexpr int PAL_MAX = 5;

struct Palette {
    char name[16];
    uint8_t n;          // 2..5 colours
    CRGB c[PAL_MAX];
    // Where each colour sits along the palette, 0..1, in order (first usually 0, last usually 1).
    // Evenly spaced by default: j / (n - 1).
    float pos[PAL_MAX];
};

// The palette as saved before colours had positions (always evenly spaced)
struct PaletteV1 {
    char name[16];
    uint8_t n;
    CRGB c[PAL_MAX];
};

void paletteEvenStops(Palette& p);           // positions j / (n - 1)
bool paletteHasEvenStops(const Palette& p);
void paletteSanitizeStops(Palette& p);       // 0..1, in order

// 256-entry lookup. wrap: the colours around a loop (for cycling effects): the stops are squeezed
// into the first (n-1)/n of the loop, and the last colour blends back into the first over the
// rest; evenly spaced, that is n equal steps. Otherwise a ramp: the first colour up to its
// position, the last from its position, blends between.
void buildLut(const Palette& p, CRGB* lut, bool wrap);
