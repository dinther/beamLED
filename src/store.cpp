#include "store.h"

#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>

Palette palettes[NUM_PALETTES];
Preset presets[MAX_PRESETS];
int presetCount = 0;
Settings settings;
Layout layout;
PresetSync presetSync[MAX_PRESETS];
// The presets as saved: presets[] is what plays (edits land there at once), these are what Save
// stores and Revert goes back to. From the heap: static DRAM is full.
static Preset* savedPresets = nullptr;
static PresetSync* savedSync = nullptr;
LinkSettings linkSettings;

static const uint32_t SCHEMA = 1;  // bump when a struct above changes
static const uint32_t SAVE_DELAY_MS = 3000;
static Preferences prefs;
static bool dirty = false;
static uint32_t dirtyAt = 0;

static void setPalette(int i, const char* name, std::initializer_list<uint32_t> cols) {
    Palette& p = palettes[i];
    strlcpy(p.name, name, sizeof(p.name));
    p.n = 0;
    for (uint32_t c : cols)
        if (p.n < PAL_MAX) p.c[p.n++] = CRGB(c);
}

void presetSetEffect(Preset& p, uint8_t effect) {
    if (effect >= NUM_EFFECTS) effect = 0;
    p.effect = effect;
    const EffectDef& e = EFFECTS[effect];
    for (int j = 0; j < MAXP; j++) p.p[j] = j < e.n ? e.params[j].def : 0;
}

static void setPreset(int i, const char* name, uint8_t effect, uint8_t palette, float sparkle,
                      std::initializer_list<float> params = {}) {
    Preset& p = presets[i];
    strlcpy(p.name, name, sizeof(p.name));
    presetSetEffect(p, effect);
    p.palette = palette;
    p.sparkle = sparkle;
    int j = 0;
    for (float v : params)
        if (j < MAXP) p.p[j++] = v;
}

static void defaults() {
    setPalette(0, "Ocean", {0x001040, 0x0060a0, 0x00c0c0, 0x80ffe0});
    setPalette(1, "Sunset", {0x400010, 0xc02000, 0xff7000, 0xff3060, 0x600080});
    setPalette(2, "Forest", {0x002000, 0x208000, 0xa0d000, 0x406000});
    setPalette(3, "Fire", {0x200000, 0xc00000, 0xff6000, 0xffc020});
    setPalette(4, "Ice", {0x000830, 0x2060ff, 0xa0e0ff, 0xffffff});
    setPalette(5, "Warm white", {0x301000, 0xff8020, 0xffc070});
    setPalette(6, "Sky", {0x0040c0, 0x4090ff, 0xe0f0ff});
    setPalette(7, "Rainbow", {0xff0000, 0xffc000, 0x00ff40, 0x0040ff, 0xc000ff});

    // effect: 0 Rolling bars, 1 Searchlights, 2 Lighthouse, 3 Clouds, 4 Sparkle
    setPreset(0, "Ocean rolls", 0, 0, 0);
    setPreset(1, "Sunset rolls", 0, 1, 0, {0.1f, 3, 0.8f, 0.2f, 1, 0.2f});
    setPreset(2, "Searchlights", 1, 7, 0);
    setPreset(3, "Warm lights", 1, 5, 0, {2, 1.8f, 0.2f, 0.05f, 0.8f});
    setPreset(4, "Lighthouse", 2, 3, 0);
    setPreset(5, "Clouds", 3, 6, 0);
    setPreset(6, "Sunset clouds", 3, 1, 0, {0.6f, 4, 0.1f, 0.3f, 0.2f, 0.6f});
    setPreset(7, "Ice + sparkle", 0, 4, 0.3f, {0.08f, 3, 0.7f, 0.4f, -1, 0.25f});
    setPreset(8, "Starry", 4, 4, 0);
    setPreset(9, "Forest clouds", 3, 2, 0.1f, {0.45f, 5, 0.08f, -0.3f, 0.15f, 0.5f});
    presetCount = 10;

    settings.brightness = 160;
    settings.current = 0;
    settings.gamma = 2.2f;
    settings.target = (uint32_t)IPAddress(cfg::DEFAULT_TARGET[0], cfg::DEFAULT_TARGET[1],
                                          cfg::DEFAULT_TARGET[2], cfg::DEFAULT_TARGET[3]);
}

void layoutSanitize(Layout& l) {
    l.nx = constrain(l.nx, 1, MAX_X);
    l.ny = constrain(l.ny, 1, MAX_Y);
    if (l.nx * l.ny > MAX_LEDS) l.ny = MAX_LEDS / l.nx;
    if (!(l.length >= 0.05f && l.length <= 200)) l.length = cfg::CEILING_LENGTH;
    if (!(l.aspect >= 0.1f && l.aspect <= 500)) l.aspect = 19.1f;
    l.measured = l.measured ? 1 : 0;
    const int upr = (l.nx + 169) / 170;
    for (int b = 0; b < MAX_Y; b++) l.rowUni[b] = constrain(l.rowUni[b], 0, 32768 - upr);
}

void layoutFillUniverses(Layout& l, int from, int start) {
    const int upr = (l.nx + 169) / 170;
    for (int b = from < 0 ? 0 : from; b < MAX_Y; b++) {
        const int u = b == 0 ? start : l.rowUni[b - 1] + upr;
        l.rowUni[b] = constrain(u, 0, 32768 - upr);
    }
}

static void layoutDefaults() {
    layout.nx = cfg::CEILING_PIX;
    layout.ny = cfg::CEILING_ROWS;
    layout.length = cfg::CEILING_LENGTH;
    // Mean measured row spacing / LED spacing: (7.42 m / 14) / (5 m / 180)
    layout.aspect = 19.1f;
    layout.measured = 1;
    layoutFillUniverses(layout, 0, 0);  // the PixLite: row n at universe 2n
}

// The layout has its own key, so changing it never resets presets or palettes
void storeSaveLayout() {
    prefs.begin("beamled", false);
    prefs.putBytes("lay", &layout, sizeof(layout));
    prefs.end();
}

// Presets are saved as a list ("pres", "syncs"); before presets could be added there were
// always 10, under "pre" and "sync". Those are read once and then removed.
static const int OLD_PRESETS = 10;

static void save() {
    prefs.begin("beamled", false);
    prefs.putUInt("ver", SCHEMA);
    prefs.putBytes("pal", palettes, sizeof(palettes));
    prefs.putBytes("pres", savedPresets, presetCount * sizeof(Preset));
    prefs.putBytes("set", &settings, sizeof(settings));
    prefs.putBytes("syncs", savedSync, presetCount * sizeof(PresetSync));
    prefs.putBytes("link", &linkSettings, sizeof(linkSettings));
    if (prefs.isKey("pre")) prefs.remove("pre");
    if (prefs.isKey("sync")) prefs.remove("sync");
    prefs.end();
    Serial.println("Settings saved");
}

static const PresetSync SYNC_DEFAULT = {TIMING_TIME, 2, 0, 0};

void storeLoad() {
    defaults();
    for (PresetSync& y : presetSync) y = SYNC_DEFAULT;
    prefs.begin("beamled", true);
    const size_t listLen = prefs.isKey("pres") ? prefs.getBytesLength("pres") : 0;
    const bool list = listLen >= sizeof(Preset) && listLen % sizeof(Preset) == 0 &&
                      listLen / sizeof(Preset) <= MAX_PRESETS;
    const bool old = !list && prefs.isKey("pre") &&
                     prefs.getBytesLength("pre") == OLD_PRESETS * sizeof(Preset);
    const bool ok = prefs.getUInt("ver", 0) == SCHEMA &&
                    prefs.getBytesLength("pal") == sizeof(palettes) &&
                    prefs.getBytesLength("set") == sizeof(settings) && (list || old);
    if (ok) {
        prefs.getBytes("pal", palettes, sizeof(palettes));
        prefs.getBytes("set", &settings, sizeof(settings));
        presetCount = list ? listLen / sizeof(Preset) : OLD_PRESETS;
        prefs.getBytes(list ? "pres" : "pre", presets, presetCount * sizeof(Preset));
        const char* syncKey = list ? "syncs" : "sync";
        if (prefs.isKey(syncKey) && prefs.getBytesLength(syncKey) == presetCount * sizeof(PresetSync))
            prefs.getBytes(syncKey, presetSync, presetCount * sizeof(PresetSync));
    }
    prefs.end();
    // Repair anything out of range
    for (Palette& p : palettes) {
        p.name[sizeof(p.name) - 1] = 0;
        p.n = constrain(p.n, 2, PAL_MAX);
    }
    for (int i = 0; i < presetCount; i++) {
        Preset& p = presets[i];
        p.name[sizeof(p.name) - 1] = 0;
        if (p.effect >= NUM_EFFECTS) presetSetEffect(p, 0);
        if (p.palette >= NUM_PALETTES) p.palette = 0;
    }
    for (PresetSync& y : presetSync) {
        if (y.timing > TIMING_BEATS) y.timing = TIMING_TIME;
        if (y.beatsExp > 6) y.beatsExp = 2;
        y.reverse = y.reverse ? 1 : 0;
        if (!(y.pulse >= 0 && y.pulse <= 1)) y.pulse = 0;
    }
    if (settings.current >= presetCount) settings.current = 0;
    if (!(settings.gamma >= 1 && settings.gamma <= 3)) settings.gamma = 2.2f;

    // Kept under their own keys: a missing or older one falls back to defaults on its own
    layoutDefaults();
    linkSettings = {0, 120, 20};
    prefs.begin("beamled", true);
    if (prefs.isKey("lay") && prefs.getBytesLength("lay") == sizeof(layout))
        prefs.getBytes("lay", &layout, sizeof(layout));
    if (prefs.isKey("link") && prefs.getBytesLength("link") == sizeof(linkSettings))
        prefs.getBytes("link", &linkSettings, sizeof(linkSettings));
    prefs.end();
    layoutSanitize(layout);
    linkSettings.enabled = linkSettings.enabled ? 1 : 0;
    if (!(linkSettings.bpm >= 20 && linkSettings.bpm <= 999)) linkSettings.bpm = 120;
    if (!(linkSettings.latencyMs >= 0 && linkSettings.latencyMs <= 500)) linkSettings.latencyMs = 20;
    if (!savedPresets) savedPresets = (Preset*)calloc(MAX_PRESETS, sizeof(Preset));
    if (!savedSync) savedSync = (PresetSync*)calloc(MAX_PRESETS, sizeof(PresetSync));
    for (int i = 0; i < MAX_PRESETS; i++) {
        savedPresets[i] = presets[i];
        savedSync[i] = presetSync[i];
    }
    if (!ok || old) save();
}

// ---- Save / Revert per preset

static bool samePreset(const Preset& a, const Preset& b) {
    if (a.effect != b.effect || a.palette != b.palette || a.sparkle != b.sparkle) return false;
    for (int j = 0; j < MAXP; j++)
        if (a.p[j] != b.p[j]) return false;
    return true;  // the name is saved at once, so it never differs
}

static bool sameSync(const PresetSync& a, const PresetSync& b) {
    return a.timing == b.timing && a.beatsExp == b.beatsExp && a.reverse == b.reverse && a.pulse == b.pulse;
}

bool presetChanged(int i) {
    return i >= 0 && i < presetCount && (!samePreset(presets[i], savedPresets[i]) || !sameSync(presetSync[i], savedSync[i]));
}

void presetSave(int i) {
    if (i < 0 || i >= presetCount) return;
    savedPresets[i] = presets[i];
    savedSync[i] = presetSync[i];
    storeMarkDirty();
}

void presetSaveAll() {
    for (int i = 0; i < presetCount; i++) presetSave(i);
}

void presetRevert(int i) {
    if (i < 0 || i >= presetCount) return;
    presets[i] = savedPresets[i];
    presetSync[i] = savedSync[i];
}

void presetRename(int i, const char* name) {
    if (i < 0 || i >= presetCount) return;
    strlcpy(presets[i].name, name, sizeof(presets[i].name));
    strlcpy(savedPresets[i].name, name, sizeof(savedPresets[i].name));
    storeMarkDirty();
}

// Adds a copy of preset `from` after the last one. Returns its index, or -1 when full.
int presetAdd(int from) {
    if (presetCount >= MAX_PRESETS) return -1;
    if (from < 0 || from >= presetCount) from = settings.current;
    const int i = presetCount++;
    presets[i] = presets[from];
    presetSync[i] = presetSync[from];
    char name[sizeof(presets[i].name)];
    snprintf(name, sizeof(name), "%.14s copy", presets[from].name);
    strlcpy(presets[i].name, name, sizeof(presets[i].name));
    savedPresets[i] = presets[i];  // a new preset is saved as it is
    savedSync[i] = presetSync[i];
    storeMarkDirty();
    return i;
}

// Removes preset i; the last one stays.
bool presetDelete(int i) {
    if (presetCount <= 1 || i < 0 || i >= presetCount) return false;
    for (int k = i; k < presetCount - 1; k++) {
        presets[k] = presets[k + 1];
        presetSync[k] = presetSync[k + 1];
        savedPresets[k] = savedPresets[k + 1];
        savedSync[k] = savedSync[k + 1];
    }
    presetCount--;
    presetSync[presetCount] = savedSync[presetCount] = SYNC_DEFAULT;
    storeMarkDirty();
    return true;
}

// Moves preset `from` to position `to`, shifting the ones between.
bool presetMove(int from, int to) {
    if (from < 0 || from >= presetCount || to < 0 || to >= presetCount || from == to) return false;
    const Preset p = presets[from], sp = savedPresets[from];
    const PresetSync y = presetSync[from], sy = savedSync[from];
    const int d = to > from ? 1 : -1;
    for (int k = from; k != to; k += d) {
        presets[k] = presets[k + d];
        presetSync[k] = presetSync[k + d];
        savedPresets[k] = savedPresets[k + d];
        savedSync[k] = savedSync[k + d];
    }
    presets[to] = p;
    presetSync[to] = y;
    savedPresets[to] = sp;
    savedSync[to] = sy;
    storeMarkDirty();
    return true;
}

void storeMarkDirty() {
    dirty = true;
    dirtyAt = millis();
}

void storePoll() {
    if (dirty && millis() - dirtyAt >= SAVE_DELAY_MS) {
        dirty = false;
        save();
    }
}
