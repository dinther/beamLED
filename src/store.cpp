#include "store.h"

#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>

Palette palettes[MAX_PALETTES];
int paletteCount = 0;
Preset presets[MAX_PRESETS];
int presetCount = 0;
Settings settings;
Layout layout;
uint8_t rowOrder[MAX_Y];  // all RGB until set
PresetSync presetSync[MAX_PRESETS];
// The presets as saved: presets[] is what plays (edits land there at once), these are what Save
// stores and Revert goes back to. From the heap: static DRAM is full.
static Preset* savedPresets = nullptr;
static PresetSync* savedSync = nullptr;
Motion (*presetMotion)[MOTIONS] = nullptr;      // what plays
static Motion (*savedMotion)[MOTIONS] = nullptr;  // as saved
Steps* presetSteps = nullptr;        // what plays
static Steps* savedSteps = nullptr;  // as saved
// Added and not saved since: its first Save takes a new name in place (it is new anyway)
static bool fresh[MAX_PRESETS];
static const Motion MOTION_OFF = {0, SHAPE_SINE, 0, 3, 0, 1, 8, 0};
LinkSettings linkSettings;
AutoSettings autoSettings;
SparkleSettings sparkleSettings;
Levels levels = {255, 255};
int8_t paletteLock = -1;

static const uint32_t SCHEMA = 1;  // bump when a struct above changes
static const uint32_t SAVE_DELAY_MS = 3000;
static Preferences prefs;
static bool dirty = false;
static uint32_t dirtyAt = 0, firstDirtyAt = 0;
static const uint32_t SAVE_LATEST_MS = 15000;  // a change waits at most this long, however busy it gets

static void setPalette(int i, const char* name, std::initializer_list<uint32_t> cols) {
    Palette& p = palettes[i];
    strlcpy(p.name, name, sizeof(p.name));
    p.n = 0;
    for (uint32_t c : cols)
        if (p.n < PAL_MAX) p.c[p.n++] = CRGB(c);
    paletteEvenStops(p);
    for (uint8_t& m : p.mid) m = 128;
}

void presetSetEffect(Preset& p, uint8_t effect) {
    if (effect >= NUM_EFFECTS) effect = 0;
    p.effect = effect;
    const EffectDef& e = EFFECTS[effect];
    for (int j = 0; j < MAXP; j++) p.p[j] = j < e.n ? e.params[j].def : 0;
}

// sparkle: what the factory preset had as its own overlay; unused since the overlay is global
static void setPreset(int i, const char* name, uint8_t effect, uint8_t palette, float sparkle,
                      std::initializer_list<float> params = {}) {
    Preset& p = presets[i];
    strlcpy(p.name, name, sizeof(p.name));
    presetSetEffect(p, effect);
    p.palette = palette;
    (void)sparkle;
    // The values are listed as they were before 2026-11-19: in the sliders' old order, lengths and
    // speeds in metres. The defaults (now in %) go back to metres first, so all convert alike.
    for (int k = 0; k < EFFECTS[effect].n; k++)
        if (EFFECTS[effect].params[k].rel) p.p[k] /= 20.f;
    effectsSwapCentres(effect, p.p);  // the defaults to the old meaning first, so all convert alike
    effectsParamsToOldOrder(effect, p.p);
    int j = 0;
    for (float v : params)
        if (j < MAXP) p.p[j++] = v;
    effectsParamsFromOldOrder(effect, p.p);
    effectsMetresToPercent(effect, p.p);
    effectsSwapCentres(effect, p.p);  // and Centre X / Y as since version 4
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
    paletteCount = 8;

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
    layoutFillUniverses(layout, 0, 0);  // row n at universe 2n
}

// The layout has its own key, so changing it never resets presets or palettes
void storeSaveLayout() {
    prefs.begin("beamled", false);
    prefs.putBytes("lay", &layout, sizeof(layout));
    prefs.putBytes("ord", rowOrder, sizeof(rowOrder));
    prefs.end();
}

// Presets are saved as a list ("pres", "syncs"); before presets could be added there were
// always 10, under "pre" and "sync". Those are read once and then removed.
static const int OLD_PRESETS = 10;

static uint32_t saves = 0;  // since power-up
uint32_t storeSaves() { return saves; }
static uint32_t marks = 0;  // storeMarkDirty calls since power-up
uint32_t storeMarks() { return marks; }
int32_t storeDirtyMs() { return dirty ? (int32_t)(millis() - dirtyAt) : -1; }

// Flash wears with every page NVS erases, and NVS erases a page each time a save fills one. So a
// save writes only what changed since the last: each part is fingerprinted, and a part with the
// same fingerprint is not written again. Selecting a preset then writes 12 bytes, not 3 KB.
enum SavePart { PART_PALS, PART_PRES, PART_SET, PART_SYNCS, PART_LINK, PART_AUTO, PART_MOTS, PART_SPARK, PART_LEVELS, PART_STEPS, NUM_PARTS };
static uint32_t partPrint[NUM_PARTS];
static bool partKnown[NUM_PARTS];
static bool verWritten = false;

static uint32_t fingerprint(const void* data, size_t len) {
    uint32_t h = 2166136261u ^ (uint32_t)len;  // FNV-1a
    for (size_t i = 0; i < len; i++) h = (h ^ ((const uint8_t*)data)[i]) * 16777619u;
    return h;
}

// Writes the part unless it is what was written (or loaded) last
static void putPart(SavePart part, const char* key, const void* data, size_t len) {
    const uint32_t h = fingerprint(data, len);
    if (partKnown[part] && partPrint[part] == h) return;
    prefs.putBytes(key, data, len);
    partPrint[part] = h;
    partKnown[part] = true;
}

// After loading: what is in RAM is what is in flash
static void rememberSaved() {
    partPrint[PART_PALS] = fingerprint(palettes, paletteCount * sizeof(Palette));
    partPrint[PART_PRES] = fingerprint(savedPresets, presetCount * sizeof(Preset));
    partPrint[PART_SET] = fingerprint(&settings, sizeof(settings));
    partPrint[PART_SYNCS] = fingerprint(savedSync, presetCount * sizeof(PresetSync));
    partPrint[PART_LINK] = fingerprint(&linkSettings, sizeof(linkSettings));
    partPrint[PART_AUTO] = fingerprint(&autoSettings, sizeof(autoSettings));
    partPrint[PART_MOTS] = fingerprint(savedMotion, presetCount * sizeof(Motion) * MOTIONS);
    partPrint[PART_SPARK] = fingerprint(&sparkleSettings, sizeof(sparkleSettings));
    partPrint[PART_LEVELS] = fingerprint(&levels, sizeof(levels));
    partPrint[PART_STEPS] = fingerprint(savedSteps, presetCount * sizeof(Steps));
    for (bool& k : partKnown) k = true;
    verWritten = true;
}

// The sliders' version: 2 = one order for every effect (2026-11-19), 3 = lengths and speeds in %
// of the ceiling's shorter side, 4 = Centre X across the bars, Y along them (both 2026-11-19).
// Missing: the order before, in metres.
static const uint8_t PARAM_ORDER = 4;
static bool orderWritten = false;

static void save() {
    saves++;
    prefs.begin("beamled", false);
    if (!verWritten) {
        prefs.putUInt("ver", SCHEMA);
        verWritten = true;
    }
    putPart(PART_PALS, "palsm", palettes, paletteCount * sizeof(Palette));  // a list, as the presets
    putPart(PART_PRES, "pres", savedPresets, presetCount * sizeof(Preset));
    putPart(PART_SET, "set", &settings, sizeof(settings));
    putPart(PART_SYNCS, "syncs", savedSync, presetCount * sizeof(PresetSync));
    putPart(PART_LINK, "link", &linkSettings, sizeof(linkSettings));
    putPart(PART_AUTO, "auto", &autoSettings, sizeof(autoSettings));
    putPart(PART_MOTS, "mots", savedMotion, presetCount * sizeof(Motion) * MOTIONS);
    putPart(PART_SPARK, "spark", &sparkleSettings, sizeof(sparkleSettings));
    putPart(PART_LEVELS, "lvl", &levels, sizeof(levels));
    putPart(PART_STEPS, "steps", savedSteps, presetCount * sizeof(Steps));
    if (!orderWritten) {  // after the presets and motions in that order
        prefs.putUChar("pord", PARAM_ORDER);
        orderWritten = true;
    }
    if (prefs.isKey("pal")) prefs.remove("pal");
    if (prefs.isKey("pals7") && prefs.isKey("pals")) prefs.remove("pals");  // up to 5 colours, moved to pals7
    if (prefs.isKey("palsm") && prefs.isKey("pals7")) prefs.remove("pals7");  // before midpoints, moved to palsm
    if (prefs.isKey("pre")) prefs.remove("pre");
    if (prefs.isKey("sync")) prefs.remove("sync");
    prefs.end();
    Serial.println("Settings saved");
}

static const PresetSync SYNC_DEFAULT = {TIMING_TIME, 2, 0, 0, 0};

void storeLoad() {
    defaults();
    for (PresetSync& y : presetSync) y = SYNC_DEFAULT;
    prefs.begin("beamled", true);
    const size_t listLen = prefs.isKey("pres") ? prefs.getBytesLength("pres") : 0;
    const bool list = listLen >= sizeof(Preset) && listLen % sizeof(Preset) == 0 &&
                      listLen / sizeof(Preset) <= MAX_PRESETS;
    const bool old = !list && prefs.isKey("pre") &&
                     prefs.getBytesLength("pre") == OLD_PRESETS * sizeof(Preset);
    // Palettes: a list of up to 7 colours with midpoints under "palsm"; before, without midpoints
    // under "pals7", up to 5 colours under "pals", or the fixed 8 under "pal" from before they
    // could be added. Converted ones get their midpoints at 0 (taken as the middle, 128).
    const size_t palMLen = prefs.isKey("palsm") ? prefs.getBytesLength("palsm") : 0;
    const bool palM = palMLen >= sizeof(Palette) && palMLen % sizeof(Palette) == 0 && palMLen / sizeof(Palette) <= MAX_PALETTES;
    const size_t palLen = !palM && prefs.isKey("pals7") ? prefs.getBytesLength("pals7") : 0;
    const bool palList = palLen >= sizeof(PaletteV3) && palLen % sizeof(PaletteV3) == 0 && palLen / sizeof(PaletteV3) <= MAX_PALETTES;
    const size_t pal5Len = !palM && !palList && prefs.isKey("pals") ? prefs.getBytesLength("pals") : 0;
    const bool pal5 = pal5Len >= sizeof(PaletteV2) && pal5Len % sizeof(PaletteV2) == 0 &&
                      pal5Len / sizeof(PaletteV2) <= MAX_PALETTES;
    const bool palOld = !palM && !palList && !pal5 && prefs.isKey("pal") && prefs.getBytesLength("pal") == 8 * sizeof(PaletteV1);
    const bool ok = prefs.getUInt("ver", 0) == SCHEMA &&
                    (palM || palList || pal5 || palOld) &&
                    prefs.getBytesLength("set") == sizeof(settings) && (list || old);
    if (ok) {
        if (palM) {
            paletteCount = palMLen / sizeof(Palette);
            prefs.getBytes("palsm", palettes, palMLen);
        } else if (palList) {  // saved before midpoints: the same colours and stops, straight blends
            PaletteV3* v3 = (PaletteV3*)malloc(palLen);  // heap, only while converting (static DRAM is full)
            if (v3) {  // (without it the default palettes stay)
                paletteCount = palLen / sizeof(PaletteV3);
                prefs.getBytes("pals7", v3, palLen);
                for (int k = 0; k < paletteCount; k++) {
                    Palette& p = palettes[k];
                    p = {};
                    memcpy(p.name, v3[k].name, sizeof(p.name));
                    p.n = v3[k].n;
                    memcpy(p.c, v3[k].c, sizeof(p.c));
                    memcpy(p.pos, v3[k].pos, sizeof(p.pos));
                }
                free(v3);
            }
        } else if (pal5) {  // saved with up to 5 colours: the same colours and stops
            PaletteV2 v2[MAX_PALETTES];
            paletteCount = pal5Len / sizeof(PaletteV2);
            prefs.getBytes("pals", v2, pal5Len);
            for (int k = 0; k < paletteCount; k++) {
                Palette& p = palettes[k];
                p = {};
                memcpy(p.name, v2[k].name, sizeof(p.name));
                p.n = v2[k].n;
                for (int j = 0; j < 5; j++) {
                    p.c[j] = v2[k].c[j];
                    p.pos[j] = v2[k].pos[j];
                }
                for (int j = 5; j < PAL_MAX; j++) p.pos[j] = 1.f;
            }
        } else {  // saved before colours had positions: evenly spaced, as they looked
            PaletteV1 v1[8];
            prefs.getBytes("pal", v1, sizeof(v1));
            paletteCount = 8;
            for (int k = 0; k < 8; k++) {
                memcpy(palettes[k].name, v1[k].name, sizeof(v1[k].name));
                palettes[k].n = v1[k].n;
                memcpy(palettes[k].c, v1[k].c, sizeof(v1[k].c));
                for (int j = 5; j < PAL_MAX; j++) palettes[k].c[j] = CRGB::Black;
                paletteEvenStops(palettes[k]);
            }
        }
        prefs.getBytes("set", &settings, sizeof(settings));
        presetCount = list ? listLen / sizeof(Preset) : OLD_PRESETS;
        prefs.getBytes(list ? "pres" : "pre", presets, presetCount * sizeof(Preset));
        const char* syncKey = list ? "syncs" : "sync";
        if (prefs.isKey(syncKey) && prefs.getBytesLength(syncKey) == presetCount * sizeof(PresetSync))
            prefs.getBytes(syncKey, presetSync, presetCount * sizeof(PresetSync));
    }
    prefs.end();
    // Repair anything out of range
    for (int k = 0; k < paletteCount; k++) {
        Palette& p = palettes[k];
        p.name[sizeof(p.name) - 1] = 0;
        p.n = constrain(p.n, 2, PAL_MAX);
        paletteSanitizeStops(p);
        paletteSanitizeMids(p);
    }
    for (int i = 0; i < presetCount; i++) {
        Preset& p = presets[i];
        p.name[sizeof(p.name) - 1] = 0;
        if (p.effect >= NUM_EFFECTS) presetSetEffect(p, 0);
        if (p.palette >= paletteCount) p.palette = 0;
        p.mirror = p.mirror ? 1 : 0;
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
    autoSettings = {0, 1, 3, 0, 60, 0};  // off; every 8 bars, or 60 s
    sparkleSettings = {0, 0, 0, 0, 1.2f};  // off
    levels = {255, 255};
    prefs.begin("beamled", true);
    if (prefs.isKey("lay") && prefs.getBytesLength("lay") == sizeof(layout))
        prefs.getBytes("lay", &layout, sizeof(layout));
    if (prefs.isKey("ord") && prefs.getBytesLength("ord") == sizeof(rowOrder))
        prefs.getBytes("ord", rowOrder, sizeof(rowOrder));
    for (uint8_t& o : rowOrder)
        if (o >= NUM_ORDERS) o = 0;
    if (prefs.isKey("link") && prefs.getBytesLength("link") == sizeof(linkSettings))
        prefs.getBytes("link", &linkSettings, sizeof(linkSettings));
    if (prefs.isKey("auto") && prefs.getBytesLength("auto") == sizeof(autoSettings))
        prefs.getBytes("auto", &autoSettings, sizeof(autoSettings));
    if (prefs.isKey("spark") && prefs.getBytesLength("spark") == sizeof(sparkleSettings))
        prefs.getBytes("spark", &sparkleSettings, sizeof(sparkleSettings));
    else if (prefs.isKey("spark") && prefs.getBytesLength("spark") == 12)  // from before life
        prefs.getBytes("spark", &sparkleSettings, 12);
    else if (prefs.isKey("spark") && prefs.getBytesLength("spark") == 8)  // from before colourStep
        prefs.getBytes("spark", &sparkleSettings, 8);
    if (prefs.isKey("lvl") && prefs.getBytesLength("lvl") == sizeof(levels))
        prefs.getBytes("lvl", &levels, sizeof(levels));
    prefs.end();
    layoutSanitize(layout);
    linkSettings.enabled = linkSettings.enabled ? 1 : 0;
    if (!(linkSettings.bpm >= 20 && linkSettings.bpm <= 999)) linkSettings.bpm = 120;
    if (!(linkSettings.latencyMs >= 0 && linkSettings.latencyMs <= 500)) linkSettings.latencyMs = 20;
    autoSettings.enabled = autoSettings.enabled ? 1 : 0;
    autoSettings.inBars = autoSettings.inBars ? 1 : 0;
    autoSettings.shuffle = autoSettings.shuffle ? 1 : 0;
    if (autoSettings.barsExp > 6) autoSettings.barsExp = 3;
    if (!(autoSettings.seconds >= 2 && autoSettings.seconds <= 3600)) autoSettings.seconds = 60;
    if (!(sparkleSettings.amount >= 0 && sparkleSettings.amount <= 1)) sparkleSettings.amount = 0;
    if ((sparkleSettings.beat & SPARKLE_GRID) > 6 || (sparkleSettings.beat & ~(SPARKLE_GRID | SPARKLE_BURST | SPARKLE_THROUGH)))
        sparkleSettings.beat = 0;
    if (sparkleSettings.colour > 1 + paletteCount) sparkleSettings.colour = 0;
    if (!(sparkleSettings.colourStep >= 0 && sparkleSettings.colourStep <= 1)) sparkleSettings.colourStep = 0;
    if (!(sparkleSettings.life >= 0.2f && sparkleSettings.life <= 5)) sparkleSettings.life = 1.2f;
    if (!savedPresets) savedPresets = (Preset*)calloc(MAX_PRESETS, sizeof(Preset));
    if (!savedSync) savedSync = (PresetSync*)calloc(MAX_PRESETS, sizeof(PresetSync));
    if (!presetMotion) presetMotion = (Motion (*)[MOTIONS])calloc(MAX_PRESETS, sizeof(Motion) * MOTIONS);
    if (!savedMotion) savedMotion = (Motion (*)[MOTIONS])calloc(MAX_PRESETS, sizeof(Motion) * MOTIONS);
    if (!presetSteps) presetSteps = (Steps*)calloc(MAX_PRESETS, sizeof(Steps));
    if (!savedSteps) savedSteps = (Steps*)calloc(MAX_PRESETS, sizeof(Steps));
    // Motions have their own key: presets saved before there were motions get none
    for (int i = 0; i < MAX_PRESETS; i++)
        for (int m = 0; m < MOTIONS; m++) presetMotion[i][m] = MOTION_OFF;
    prefs.begin("beamled", true);
    bool motsOld = false;  // saved when there were 2 motions per preset: the third starts off
    if (ok && prefs.isKey("mots")) {
        const size_t len = prefs.getBytesLength("mots");
        if (len == presetCount * sizeof(Motion) * MOTIONS)
            prefs.getBytes("mots", presetMotion, len);
        else if (len == presetCount * sizeof(Motion) * 2) {
            Motion* two = (Motion*)malloc(len);
            if (two) {
                prefs.getBytes("mots", two, len);
                for (int i = 0; i < presetCount; i++)
                    for (int m = 0; m < 2; m++) presetMotion[i][m] = two[i * 2 + m];
                free(two);
                motsOld = true;
            }
        }
    }
    // Step patterns have their own key: presets saved before there were any get none
    for (int i = 0; i < MAX_PRESETS; i++) presetSteps[i] = Steps{};
    bool stepsMissing = ok;
    if (ok && prefs.isKey("steps") && prefs.getBytesLength("steps") == presetCount * sizeof(Steps)) {
        prefs.getBytes("steps", presetSteps, presetCount * sizeof(Steps));
        stepsMissing = false;
    }
    // Presets saved in the sliders' old order and in metres: moved to the order now, then to %,
    // once (and saved so)
    const uint8_t pord = ok ? prefs.getUChar("pord", 1) : PARAM_ORDER;
    const bool reorder = ok && pord < 2, percent = ok && pord < 3, centres = ok && pord < 4;
    prefs.end();
    if (reorder) {
        for (int i = 0; i < presetCount; i++) {
            effectsParamsFromOldOrder(presets[i].effect, presets[i].p);
            for (Motion& mo : presetMotion[i])
                if (mo.target) mo.target = effectsParamFromOldOrder(presets[i].effect, mo.target - 1) + 1;
        }
        Serial.println("Presets moved to the new slider order");
    }
    if (percent) {
        for (int i = 0; i < presetCount; i++) {
            const int e = presets[i].effect;
            effectsMetresToPercent(e, presets[i].p);
            for (Motion& mo : presetMotion[i])
                if (mo.target) {
                    mo.from = effectsMetresToPercent(e, mo.target - 1, mo.from);
                    mo.to = effectsMetresToPercent(e, mo.target - 1, mo.to);
                }
        }
        Serial.println("Presets' lengths and speeds moved to %");
    }
    if (centres) {
        for (int i = 0; i < presetCount; i++) {
            const int e = presets[i].effect;
            effectsSwapCentres(e, presets[i].p);
            for (Motion& mo : presetMotion[i])
                if (mo.target) mo.target = effectsSwapCentreParam(e, mo.target - 1) + 1;
        }
        Serial.println("Presets' Centre X and Y swapped (X across the bars now)");
    }
    orderWritten = ok && !reorder && !percent && !centres;
    for (int i = 0; i < presetCount; i++)
        for (Motion& mo : presetMotion[i]) {
            if (mo.target > EFFECTS[presets[i].effect].n || mo.shape >= NUM_SHAPES) mo = MOTION_OFF;
            mo.inBeats = mo.inBeats ? 1 : 0;
            if (mo.beatsExp > 6) mo.beatsExp = 3;
            if (!(mo.seconds >= 0.25f && mo.seconds <= 600)) mo.seconds = 8;
            if (!(mo.phase >= 0 && mo.phase <= 1)) mo.phase = 0;
            if (!isfinite(mo.from) || !isfinite(mo.to)) mo = MOTION_OFF;
        }
    for (int i = 0; i < MAX_PRESETS; i++) {
        savedPresets[i] = presets[i];
        savedSync[i] = presetSync[i];
        for (int m = 0; m < MOTIONS; m++) savedMotion[i][m] = presetMotion[i][m];
        savedSteps[i] = presetSteps[i];
    }
    // Saved as loaded: later saves write only what changes. Anything converted or new is written now.
    if (ok && !old && !palOld) rememberSaved();
    if (ok && !old && !palOld && stepsMissing) {  // no patterns saved yet: write the empty ones once
        partKnown[PART_STEPS] = false;
        storeMarkDirty();
    }
    if (ok && !old && !palOld && motsOld) {  // write the motions in the new size once
        partKnown[PART_MOTS] = false;
        storeMarkDirty();
    }
    if ((reorder || percent || centres) && !old && !palOld) {  // presets and motions converted: write them once
        partKnown[PART_PRES] = false;
        partKnown[PART_MOTS] = false;
        storeMarkDirty();
    }
    if (ok && (pal5 || palList)) {  // moved from an older palette format: write them under palsm once
        partKnown[PART_PALS] = false;
        storeMarkDirty();
    }
    else save();
}

// ---- Save / Revert per preset

static bool samePreset(const Preset& a, const Preset& b) {
    if (a.effect != b.effect || a.palette != b.palette || a.mirror != b.mirror)
        return false;
    for (int j = 0; j < MAXP; j++)
        if (a.p[j] != b.p[j]) return false;
    return strncmp(a.name, b.name, sizeof(a.name)) == 0;  // a new name is an edit too
}

static bool sameMotion(const Motion& a, const Motion& b) {
    return a.target == b.target && (!a.target || (a.shape == b.shape && a.inBeats == b.inBeats && a.beatsExp == b.beatsExp &&
           a.from == b.from && a.to == b.to && a.seconds == b.seconds && a.phase == b.phase));
}

static bool sameSync(const PresetSync& a, const PresetSync& b) {
    return a.timing == b.timing && a.beatsExp == b.beatsExp && a.reverse == b.reverse && a.pulse == b.pulse;
}

bool presetChanged(int i) {
    if (i < 0 || i >= presetCount) return false;
    if (!samePreset(presets[i], savedPresets[i]) || !sameSync(presetSync[i], savedSync[i])) return true;
    for (int m = 0; m < MOTIONS; m++)
        if (!sameMotion(presetMotion[i][m], savedMotion[i][m])) return true;
    return memcmp(&presetSteps[i], &savedSteps[i], sizeof(Steps)) != 0;
}

void stepsDefault(Steps& s) {
    s = Steps{};
    for (int b = 0; b < SEQ_STEPS; b += 4) s.lane[0] |= 1u << b;  // Whole on every beat
}

// Save. With a new name, the edits become a new preset of that name (added at the end), and
// preset i goes back to what was saved: its index is returned. Otherwise i itself is saved.
// inPlace: always save i itself (the new name too).
int presetSave(int i, bool inPlace) {
    if (i < 0 || i >= presetCount) return -1;
    if (!inPlace && !fresh[i] && strncmp(presets[i].name, savedPresets[i].name, sizeof(presets[i].name)) != 0) {
        char name[sizeof(presets[i].name)];
        strlcpy(name, presets[i].name, sizeof(name));
        const int k = presetAdd(i);  // a copy of what plays, edits and motions included
        if (k < 0) return -1;
        strlcpy(presets[k].name, name, sizeof(presets[k].name));
        strlcpy(savedPresets[k].name, name, sizeof(savedPresets[k].name));
        presetRevert(i);
        fresh[k] = false;  // saved, under its own name
        return k;
    }
    fresh[i] = false;
    savedPresets[i] = presets[i];
    savedSync[i] = presetSync[i];
    for (int m = 0; m < MOTIONS; m++) savedMotion[i][m] = presetMotion[i][m];
    savedSteps[i] = presetSteps[i];
    storeMarkDirty();
    return i;
}

bool presetFresh(int i) { return i >= 0 && i < presetCount && fresh[i]; }

void storeSaveNow() { save(); }

void presetSaveAll() {
    for (int i = 0; i < presetCount; i++) presetSave(i, true);
}

void presetRevert(int i) {
    if (i < 0 || i >= presetCount) return;
    presets[i] = savedPresets[i];
    presetSync[i] = savedSync[i];
    for (int m = 0; m < MOTIONS; m++) presetMotion[i][m] = savedMotion[i][m];
    presetSteps[i] = savedSteps[i];
}

void presetChangeEffect(int i, uint8_t effect) {
    if (i < 0 || i >= presetCount) return;
    presetSetEffect(presets[i], effect);
    for (int m = 0; m < MOTIONS; m++) presetMotion[i][m] = MOTION_OFF;  // their sliders are gone
    if (EFFECTS[presets[i].effect].steps) {  // a drum machine: in Beats (8 per pattern, so 1/16 steps)
        presetSync[i].timing = TIMING_BEATS;
        presetSync[i].beatsExp = 3;
        bool any = false;  // an empty pattern does nothing: start with a flash on every beat
        for (uint32_t l : presetSteps[i].lane) any |= l != 0;
        if (!any) stepsDefault(presetSteps[i]);
    }
}

// A new name is an edit: saved with Save (as a new preset), dropped with Revert
void presetRename(int i, const char* name) {
    if (i < 0 || i >= presetCount) return;
    strlcpy(presets[i].name, name, sizeof(presets[i].name));
}

const char* presetSavedName(int i) { return i >= 0 && i < presetCount ? savedPresets[i].name : ""; }

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
    for (int m = 0; m < MOTIONS; m++) presetMotion[i][m] = presetMotion[from][m];
    presetSteps[i] = presetSteps[from];
    fresh[i] = true;
    autoSettings.skip &= ~(1u << i);  // a new preset is in Auto
    savedPresets[i] = presets[i];  // a new preset is saved as it is
    savedSync[i] = presetSync[i];
    for (int m = 0; m < MOTIONS; m++) savedMotion[i][m] = presetMotion[i][m];
    savedSteps[i] = presetSteps[i];
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
        fresh[k] = fresh[k + 1];
        for (int m = 0; m < MOTIONS; m++) {
            presetMotion[k][m] = presetMotion[k + 1][m];
            savedMotion[k][m] = savedMotion[k + 1][m];
        }
        presetSteps[k] = presetSteps[k + 1];
        savedSteps[k] = savedSteps[k + 1];
    }
    presetCount--;
    presetSteps[presetCount] = savedSteps[presetCount] = Steps{};
    presetSync[presetCount] = savedSync[presetCount] = SYNC_DEFAULT;
    fresh[presetCount] = false;
    {  // the In Auto bits after i move down one
        const uint32_t s = autoSettings.skip, low = s & ((1u << i) - 1);
        autoSettings.skip = low | ((s >> (i + 1)) << i);
    }
    storeMarkDirty();
    return true;
}

// Moves preset `from` to position `to`, shifting the ones between.
bool presetMove(int from, int to) {
    if (from < 0 || from >= presetCount || to < 0 || to >= presetCount || from == to) return false;
    Motion mo[MOTIONS], smo[MOTIONS];
    for (int m = 0; m < MOTIONS; m++) { mo[m] = presetMotion[from][m]; smo[m] = savedMotion[from][m]; }
    const bool fr = fresh[from];
    const Steps st = presetSteps[from], sst = savedSteps[from];
    const Preset p = presets[from], sp = savedPresets[from];
    const PresetSync y = presetSync[from], sy = savedSync[from];
    const int d = to > from ? 1 : -1;
    for (int k = from; k != to; k += d) {
        presets[k] = presets[k + d];
        presetSync[k] = presetSync[k + d];
        savedPresets[k] = savedPresets[k + d];
        savedSync[k] = savedSync[k + d];
        fresh[k] = fresh[k + d];
        for (int m = 0; m < MOTIONS; m++) {
            presetMotion[k][m] = presetMotion[k + d][m];
            savedMotion[k][m] = savedMotion[k + d][m];
        }
        presetSteps[k] = presetSteps[k + d];
        savedSteps[k] = savedSteps[k + d];
    }
    presets[to] = p;
    presetSteps[to] = st;
    savedSteps[to] = sst;
    presetSync[to] = y;
    savedPresets[to] = sp;
    savedSync[to] = sy;
    fresh[to] = fr;
    {  // the In Auto bit moves with the preset
        uint32_t s = autoSettings.skip;
        const uint32_t b = s >> from & 1;
        const int d2 = to > from ? 1 : -1;
        for (int k = from; k != to; k += d2) s = (s & ~(1u << k)) | ((s >> (k + d2) & 1) << k);
        autoSettings.skip = (s & ~(1u << to)) | (b << to);
    }
    for (int m = 0; m < MOTIONS; m++) { presetMotion[to][m] = mo[m]; savedMotion[to][m] = smo[m]; }
    storeMarkDirty();
    return true;
}

// ---- Motion shapes: phase 0..1 is one there-and-back; 0 at From, 1 at To

static float bounceOut(float t) {
    const float n = 7.5625f, d = 2.75f;
    if (t < 1 / d) return n * t * t;
    if (t < 2 / d) { t -= 1.5f / d; return n * t * t + 0.75f; }
    if (t < 2.5f / d) { t -= 2.25f / d; return n * t * t + 0.9375f; }
    t -= 2.625f / d;
    return n * t * t + 0.984375f;
}

static float easeInOut(float x) { return x < 0.5f ? 4 * x * x * x : 1 - powf(-2 * x + 2, 3) * 0.5f; }

// Random: a value 0..1 for half-cycle n, the same everywhere (the page has this hash too)
static float randomAt(uint32_t n, int m) {
    uint32_t x = n * 2654435761u + (uint32_t)m * 40503u + 12345u;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (x >> 8) / 16777216.f;
}

float motionShape(uint8_t shape, float ph, uint32_t cycle, int m) {
    ph -= floorf(ph);
    if (shape == SHAPE_MINMAX) return ph < 0.5f ? 0.f : 1.f;  // From, then To: no transition
    if (shape == SHAPE_RANDOM) {  // to a new random value each half cycle, eased
        const uint32_t n = cycle * 2 + (ph < 0.5f ? 0 : 1);
        const float a = randomAt(n - 1, m), b = randomAt(n, m), u = ph < 0.5f ? ph * 2 : ph * 2 - 1;
        return a + (b - a) * easeInOut(u);
    }
    if (shape == SHAPE_JUMP)  // Random's values, but jumped to at once and held for each half cycle
        return randomAt(cycle * 2 + (ph < 0.5f ? 0 : 1), m);
    if (shape == SHAPE_SINE) return 0.5f - 0.5f * cosf(6.2831853f * ph);
    if (shape == SHAPE_BOUNCE) return ph < 0.5f ? bounceOut(2 * ph) : 1 - bounceOut(2 * ph - 1);
    const float x = ph < 0.5f ? 2 * ph : 2 - 2 * ph;  // there, then back
    switch (shape) {
        case SHAPE_LINEAR: return x;
        case SHAPE_EASE: return easeInOut(x);
        case SHAPE_HOLD: return easeInOut(constrain((x - 0.15f) / 0.7f, 0.f, 1.f));  // rests at both ends
        case SHAPE_OVERSHOOT: {  // a little past each end before settling
            const float c = 1.70158f * 1.525f;
            return x < 0.5f ? (4 * x * x * ((c + 1) * 2 * x - c)) * 0.5f
                            : ((2 * x - 2) * (2 * x - 2) * ((c + 1) * (2 * x - 2) + c) + 2) * 0.5f;
        }
    }
    return x;
}

// ---- The palette list

// Adds a copy of palette rom at the end. Its index, or -1 when full.
int paletteAdd(int from) {
    if (paletteCount >= MAX_PALETTES) return -1;
    if (from < 0 || from >= paletteCount) from = 0;
    const int k = paletteCount++;
    palettes[k] = palettes[from];
    char name[sizeof(palettes[k].name)];
    snprintf(name, sizeof(name), "%.10s copy", palettes[from].name);
    strlcpy(palettes[k].name, name, sizeof(palettes[k].name));
    storeMarkDirty();
    return k;
}

// Removes palette k; the last one stays. Presets that used it take the first palette, and the
// numbers of the palettes after it move down by one (in the saved presets too).
bool paletteDelete(int k) {
    if (paletteCount <= 1 || k < 0 || k >= paletteCount) return false;
    for (int j = k; j < paletteCount - 1; j++) palettes[j] = palettes[j + 1];
    paletteCount--;
    auto fix = [&](Preset& p) {
        if (p.palette == k) p.palette = 0;
        else if (p.palette > k) p.palette--;
    };
    for (int i = 0; i < presetCount; i++) {
        fix(presets[i]);
        fix(savedPresets[i]);
    }
    if (paletteLock == k) paletteLock = -1;  // its palette is gone: each preset's own again
    else if (paletteLock > k) paletteLock--;
    uint8_t& sc = sparkleSettings.colour;  // 2 + k: that palette
    if (sc >= 2 && sc - 2 == k) sc = 2;
    else if (sc >= 2 && sc - 2 > k) sc--;
    storeMarkDirty();
    return true;
}

void storeMarkDirty() {
    marks++;
    if (!dirty) firstDirtyAt = millis();
    dirty = true;
    dirtyAt = millis();
}

void storePoll() {
    // A few seconds after the last change, but never later than SAVE_LATEST_MS after the first
    if (dirty && (millis() - dirtyAt >= SAVE_DELAY_MS || millis() - firstDirtyAt >= SAVE_LATEST_MS)) {
        dirty = false;
        save();
    }
}
