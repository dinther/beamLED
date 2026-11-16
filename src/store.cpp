#include "store.h"

#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>

Palette palettes[NUM_PALETTES];
Preset presets[NUM_PRESETS];
Settings settings;
Layout layout;

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

static void save() {
    prefs.begin("beamled", false);
    prefs.putUInt("ver", SCHEMA);
    prefs.putBytes("pal", palettes, sizeof(palettes));
    prefs.putBytes("pre", presets, sizeof(presets));
    prefs.putBytes("set", &settings, sizeof(settings));
    prefs.end();
    Serial.println("Settings saved");
}

void storeLoad() {
    defaults();
    prefs.begin("beamled", true);
    const bool ok = prefs.getUInt("ver", 0) == SCHEMA &&
                    prefs.getBytesLength("pal") == sizeof(palettes) &&
                    prefs.getBytesLength("pre") == sizeof(presets) &&
                    prefs.getBytesLength("set") == sizeof(settings);
    if (ok) {
        prefs.getBytes("pal", palettes, sizeof(palettes));
        prefs.getBytes("pre", presets, sizeof(presets));
        prefs.getBytes("set", &settings, sizeof(settings));
    }
    prefs.end();
    // Repair anything out of range
    for (Palette& p : palettes) {
        p.name[sizeof(p.name) - 1] = 0;
        p.n = constrain(p.n, 2, PAL_MAX);
    }
    for (Preset& p : presets) {
        p.name[sizeof(p.name) - 1] = 0;
        if (p.effect >= NUM_EFFECTS) presetSetEffect(p, 0);
        if (p.palette >= NUM_PALETTES) p.palette = 0;
    }
    if (settings.current >= NUM_PRESETS) settings.current = 0;
    if (!(settings.gamma >= 1 && settings.gamma <= 3)) settings.gamma = 2.2f;
    if (!ok) save();

    layoutDefaults();
    prefs.begin("beamled", true);
    if (prefs.isKey("lay") && prefs.getBytesLength("lay") == sizeof(layout))
        prefs.getBytes("lay", &layout, sizeof(layout));
    prefs.end();
    layoutSanitize(layout);
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
