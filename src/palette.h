#pragma once
#include <FastLED.h>

constexpr int PAL_MAX = 7;

struct Palette {
    char name[16];
    uint8_t n;          // 2..7 colours
    CRGB c[PAL_MAX];
    // Where each colour sits along the palette, 0..1, in order (first usually 0, last usually 1).
    // Evenly spaced by default: j / (n - 1).
    float pos[PAL_MAX];
    // Where the blend from colour j to colour j + 1 is half-way, as a share of that stretch, in
    // 256ths: 128 = the middle (a straight blend). Further from 128 the blend holds one colour
    // longer and turns to the next more quickly, as a CSS colour hint. 0 = not set (taken as 128).
    uint8_t mid[PAL_MAX];
};

// The palette as saved with up to 7 colours before midpoints (under "pals7"; now "palsm")
struct PaletteV3 {
    char name[16];
    uint8_t n;
    CRGB c[PAL_MAX];
    float pos[PAL_MAX];
};

// The palette as saved before colours had positions (always evenly spaced)
struct PaletteV1 {
    char name[16];
    uint8_t n;
    CRGB c[5];
};

// The palette as saved with up to 5 colours (under "pals"; now 7, under "pals7")
struct PaletteV2 {
    char name[16];
    uint8_t n;
    CRGB c[5];
    float pos[5];
};

void paletteEvenStops(Palette& p);           // positions j / (n - 1)
bool paletteHasEvenStops(const Palette& p);
void paletteSanitizeStops(Palette& p);       // 0..1, in order
void paletteSanitizeMids(Palette& p);        // 0 (not set) -> 128, the rest kept within 5..251

// 256-entry lookup. wrap: the colours around a loop (for cycling effects): the stops are squeezed
// into the first (n-1)/n of the loop, and the last colour blends back into the first over the
// rest; evenly spaced, that is n equal steps. Otherwise a ramp: the first colour up to its
// position, the last from its position, blends between.
void buildLut(const Palette& p, CRGB* lut, bool wrap);
