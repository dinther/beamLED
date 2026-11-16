#pragma once
#include <FastLED.h>

constexpr int PAL_MAX = 5;

struct Palette {
    char name[16];
    uint8_t n;          // 2..5 colours
    CRGB c[PAL_MAX];
};

// 256-entry lookup. wrap: colours evenly spaced around a loop (for cycling effects);
// otherwise first colour at 0 and last at 255 (for ramps such as sky -> cloud).
void buildLut(const Palette& p, CRGB* lut, bool wrap);
