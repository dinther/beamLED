// Presets, palettes and settings, kept in NVS. Changes are saved a few seconds after the
// last edit, so dragging a slider doesn't wear the flash.
#pragma once
#include "effects.h"
#include "palette.h"
#include "layout.h"
#include "config.h"

constexpr int MAX_PRESETS = 32;  // presets can be added up to this many
constexpr int MAX_PALETTES = 16;  // palettes can be added up to this many

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

// A Motion moves one of the preset's sliders back and forth by itself, between From and To,
// along a shape. Its period is one full there-and-back, in seconds or in beats (then it is on the
// beat grid: From on the bar at phase 0). It doesn't change the slider's own value.
enum MotionShape : uint8_t { SHAPE_SINE, SHAPE_LINEAR, SHAPE_EASE, SHAPE_HOLD, SHAPE_OVERSHOOT, SHAPE_BOUNCE,
                             SHAPE_RANDOM, SHAPE_MINMAX, NUM_SHAPES };
struct Motion {
    uint8_t target;    // 0 = off, else the effect's parameter + 1
    uint8_t shape;     // MotionShape
    uint8_t inBeats;   // the period in beats (2^beatsExp) instead of seconds
    uint8_t beatsExp;  // 0..6 = 1, 2, 4 ... 64 beats
    float from, to;    // the two ends, in the parameter's units
    float seconds;     // the period in seconds
    float phase;       // 0..1: where in the cycle it starts
};
constexpr int MOTIONS = 2;
// 0..1 (Overshoot goes a little beyond). cycle: how many whole cycles have gone by (Random's values
// come from it, so they are the same on every run and on the page); m: which motion (its own values).
float motionShape(uint8_t shape, float phase, uint32_t cycle = 0, int m = 0);

struct LinkSettings {
    uint8_t enabled;  // Ableton Link
    float bpm;        // own tempo when no peers (and the last tempo seen)
    float latencyMs;  // frames are rendered this much ahead, to land on the beat at the LEDs
};

extern Palette palettes[MAX_PALETTES];
extern int paletteCount;  // 1..MAX_PALETTES
extern Preset presets[MAX_PRESETS];
extern PresetSync presetSync[MAX_PRESETS];
extern int presetCount;  // 1..MAX_PRESETS
extern Motion (*presetMotion)[MOTIONS];  // [MAX_PRESETS][MOTIONS], from the heap
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
// Edits to a preset (sliders, effect, palette, timing, OSC) play at once but aren't saved until
// presetSave; presetRevert goes back to the saved version. A new name is an edit too: saving it
// makes a new preset of that name, and the original goes back to what was saved.
bool presetChanged(int i);   // differs from its saved version
int presetSave(int i, bool inPlace = false);  // the index that was saved (a new one for a new name), or -1
void presetSaveAll();
void presetRevert(int i);
void presetRename(int i, const char* name);  // an edit, until Save
const char* presetSavedName(int i);
bool presetFresh(int i);  // added and not saved since: Save keeps a new name in place
// Another effect for preset i: its parameters to that effect's defaults, its motions off
void presetChangeEffect(int i, uint8_t effect);
// The palette list (saved a few seconds later)
int paletteAdd(int from);   // a copy of palette 'from' at the end; its index, or -1 when full
bool paletteDelete(int k);  // never the last one; presets using it take the first palette
