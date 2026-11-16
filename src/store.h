// Presets, palettes and settings, kept in NVS. Changes are saved a few seconds after the
// last edit, so dragging a slider doesn't wear the flash.
#pragma once
#include "effects.h"
#include "palette.h"
#include "layout.h"
#include "config.h"

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

// Beat sync per preset (stored apart from Preset so adding it kept existing presets)
struct PresetSync {
    uint8_t tempo;  // speeds follow the tempo (the set speed applies at 120 BPM)
    uint8_t lock;   // cyclic motion locked to the beat grid
    float pulse;    // brightness accent on each beat, 0..1
};

struct LinkSettings {
    uint8_t enabled;  // Ableton Link
    float bpm;        // own tempo when no peers (and the last tempo seen)
    float latencyMs;  // frames are rendered this much ahead, to land on the beat at the LEDs
};

extern Palette palettes[NUM_PALETTES];
extern Preset presets[NUM_PRESETS];
extern PresetSync presetSync[NUM_PRESETS];
extern Settings settings;
extern LinkSettings linkSettings;

void storeLoad();
void storeMarkDirty();
void storePoll();
void storeSaveLayout();
void presetSetEffect(Preset& p, uint8_t effect);
