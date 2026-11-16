// Presets, palettes and settings, kept in NVS. Changes are saved a few seconds after the
// last edit, so dragging a slider doesn't wear the flash.
#pragma once
#include "effects.h"
#include "palette.h"
#include "layout.h"
#include "config.h"

constexpr int MAX_PRESETS = 32;  // presets can be added up to this many
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
enum Timing : uint8_t {
    TIMING_TIME,    // speeds in their own units (m/s, rpm, ...)
    TIMING_TEMPO,   // the same, scaled by bpm / 120
    TIMING_BEATS,   // the effect's cycle takes 2^beatsExp beats, locked to the beat grid
};

struct PresetSync {
    uint8_t timing;    // Timing
    uint8_t beatsExp;  // timing in beats: 0..6 = 1, 2, 4, ... 64 beats per cycle
    uint8_t reverse;   // timing in beats: cycle runs backwards
    float pulse;       // brightness accent on each beat, 0..1
};

struct LinkSettings {
    uint8_t enabled;  // Ableton Link
    float bpm;        // own tempo when no peers (and the last tempo seen)
    float latencyMs;  // frames are rendered this much ahead, to land on the beat at the LEDs
};

extern Palette palettes[NUM_PALETTES];
extern Preset presets[MAX_PRESETS];
extern PresetSync presetSync[MAX_PRESETS];
extern int presetCount;  // 1..MAX_PRESETS
extern Settings settings;
extern LinkSettings linkSettings;

void storeLoad();
void storeMarkDirty();
void storePoll();
void storeSaveLayout();
void presetSetEffect(Preset& p, uint8_t effect);
// Editing the list (saved a few seconds later). The engine has to follow: see engine.h.
int presetAdd(int from);          // a copy of preset 'from' at the end; its index, or -1 when full
bool presetDelete(int i);         // never the last one
bool presetMove(int from, int to);
