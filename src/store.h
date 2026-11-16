// Presets, palettes and settings, kept in NVS. Changes are saved a few seconds after the
// last edit, so dragging a slider doesn't wear the flash.
#pragma once
#include "effects.h"
#include "palette.h"

constexpr int NUM_PRESETS = 10;
constexpr int NUM_PALETTES = 8;

struct Preset {
    char name[20];
    uint8_t effect;
    uint8_t palette;
    float p[MAXP];
    float sparkle;  // overlay density 0..1
};

struct Settings {
    uint8_t brightness;  // 0..255
    uint8_t current;     // preset; survives a power cut
    float gamma;
    uint32_t target;     // Art-Net destination (IPAddress as uint32)
};

extern Palette palettes[NUM_PALETTES];
extern Preset presets[NUM_PRESETS];
extern Settings settings;

void storeLoad();
void storeMarkDirty();
void storePoll();
void presetSetEffect(Preset& p, uint8_t effect);
