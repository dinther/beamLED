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
    uint8_t unusedSparkleBeat;  // was the preset's own Sparkle overlay timing; the overlay is
                                // global now (SparkleSettings). Kept so saved presets still load.
    uint8_t mirror;       // the right half mirrors the left (only the left is rendered; in what was
                          // padding: older saves read as 0)
    float p[MAXP];
    float unusedSparkle;  // was the preset's own Sparkle overlay amount (see unusedSparkleBeat)
};
static_assert(sizeof(Preset) == 60, "Preset is saved as is: its size must not change");
constexpr uint8_t SPARKLE_GRID = 0x07, SPARKLE_BURST = 0x80;

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
    uint8_t unusedSparkleColour;  // was the preset's own Sparkle overlay colours (now global)
    float pulse;       // brightness accent on each beat, 0..1
};
static_assert(sizeof(PresetSync) == 8, "PresetSync is saved as is: its size must not change");

// A Motion moves one of the preset's sliders back and forth by itself, between From and To,
// along a shape. Its period is one full there-and-back, in seconds or in beats (then it is on the
// beat grid: From on the bar at phase 0). It doesn't change the slider's own value.
enum MotionShape : uint8_t { SHAPE_SINE, SHAPE_LINEAR, SHAPE_EASE, SHAPE_HOLD, SHAPE_OVERSHOOT, SHAPE_BOUNCE,
                             SHAPE_RANDOM, SHAPE_MINMAX, SHAPE_JUMP, NUM_SHAPES };
struct Motion {
    uint8_t target;    // 0 = off, else the effect's parameter + 1
    uint8_t shape;     // MotionShape
    uint8_t inBeats;   // the period in beats (2^beatsExp) instead of seconds
    uint8_t beatsExp;  // 0..6 = 1, 2, 4 ... 64 beats
    float from, to;    // the two ends, in the parameter's units
    float seconds;     // the period in seconds
    float phase;       // 0..1: where in the cycle it starts
};
constexpr int MOTIONS = 3;  // saves from when there were 2 load with the third off
// 0..1 (Overshoot goes a little beyond). cycle: how many whole cycles have gone by (Random's values
// come from it, so they are the same on every run and on the page); m: which motion (its own values).
float motionShape(uint8_t shape, float phase, uint32_t cycle = 0, int m = 0);

// Auto: the presets change by themselves, every so many seconds or bars (on the bar line), in order
// or shuffled, among the presets that are in it.
struct AutoSettings {
    uint8_t enabled;
    uint8_t inBars;   // the interval in bars (2^barsExp) instead of seconds
    uint8_t barsExp;  // 0..6 = 1, 2, 4 ... 64 bars
    uint8_t shuffle;  // a random preset each time, not the next
    float seconds;    // the interval in seconds
    uint32_t skip;    // presets left out, one bit each
};

// The Sparkle overlay: twinkles on top of whatever preset plays (not on the Sparkle effect, which
// is twinkles itself). One for all presets, kept across preset changes.
struct SparkleSettings {
    float amount;    // 0 = off .. 1 = about 8% of the LEDs lit at a time
    uint8_t beat;    // low bits (SPARKLE_GRID): 0 = free; new twinkles start only every 1 = 1 beat,
                     // 2 = 1/2, 3 = 1/4, 4 = 1/8 beat, 5 = 2 beats, 6 = 4 beats (codes kept from when
                     // 1..4 were called 1/4 .. 1/32 notes); SPARKLE_BURST: short flashes, gone before the next tick
    uint8_t colour;  // 0 = white, 1 = the playing preset's palette, 2 + k = palette k
    float colourStep;  // 0 = random colours; else each new twinkle this far along the palette
                       // from the one before (added later: an 8-byte save loads it as 0)
};

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
extern AutoSettings autoSettings;
extern SparkleSettings sparkleSettings;
// Palette lock: -1 = every preset plays its own palette, k = every preset plays palette k (its
// own setting untouched). Never saved: after a restart each preset has its own again.
extern int8_t paletteLock;
inline bool presetInAuto(int i) { return !(autoSettings.skip >> i & 1); }

void storeLoad();
void storeMarkDirty();
void storePoll();
uint32_t storeSaves();  // saves since power-up (for wear checks)
uint32_t storeMarks();  // changes marked for saving since power-up
int32_t storeDirtyMs(); // ms since the last change waiting to be saved, -1 = nothing waiting
void storeSaveLayout();
void storeSaveNow();  // write what changed straight away (before a restart)
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
