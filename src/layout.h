// The LED grid: rows of LEDs, set on the web page. Effects work in metres: x along a row
// (LED spacing = row length / LEDs per row), y across the rows (row spacing = aspect x LED
// spacing, or the measured rows).
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
    uint8_t measured;  // use the measured rows (15 rows only)
    // Art-Net start universe of each row. A row uses universesPerRow() consecutive universes
    // from there (170 RGB pixels each).
    uint16_t rowUni[MAX_Y];
};

extern Layout layout;

// Colour order of each row's LEDs as the pixel controller expects them: 0 RGB, 1 RBG, 2 GRB,
// 3 GBR, 4 BRG, 5 BGR. Kept apart from Layout (its own key), so stored layouts stay as they are.
constexpr int NUM_ORDERS = 6;
extern uint8_t rowOrder[MAX_Y];

inline int layoutLeds() { return layout.nx * layout.ny; }
inline int universesPerRow() { return (layout.nx + 169) / 170; }
void layoutSanitize(Layout& l);
// Rows from `from` on get consecutive universes after row from-1 (or from `start` for row 0)
void layoutFillUniverses(Layout& l, int from, int start = 0);
