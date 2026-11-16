// The LED grid: rows of LEDs, set on the web page. Effects work in metres: x along a row
// (LED spacing = row length / LEDs per row), y across the rows (row spacing = aspect x LED
// spacing, or the measured ceiling rows).
#pragma once
#include <stdint.h>

constexpr int MAX_X = 512;     // LEDs per row
constexpr int MAX_Y = 64;      // rows
constexpr int MAX_LEDS = 6144; // 36 KB with the sparkle state

struct Layout {
    uint16_t nx;       // LEDs per row
    uint16_t ny;       // rows
    float length;      // m, row length
    float aspect;      // row spacing / LED spacing
    uint8_t measured;  // use the measured ceiling rows (15 rows only)
    uint16_t startUni; // Art-Net universe of row 0; each row starts on a new universe
};

extern Layout layout;

inline int layoutLeds() { return layout.nx * layout.ny; }
inline int universesPerRow() { return (layout.nx + 169) / 170; }
void layoutSanitize(Layout& l);
