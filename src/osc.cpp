#include "osc.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_http_server.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <math.h>

#include "config.h"
#include "effects.h"
#include "engine.h"
#include "link_clock.h"
#include "net.h"
#include "store.h"
#include "web.h"

// ---------------------------------------------------------------- the address space
// Every address has a fixed number (node id), so feedback can remember values and
// subscriptions in small arrays: the globals, then MAX_PRESETS blocks of PSLOTS, then
// MAX_PALETTES blocks of CSLOTS. A preset's parameters take the slots after its fixed ones.

enum Global : uint8_t { G_PRESET, G_NEXT, G_PREV, G_ON, G_BRIGHTNESS, G_LINK, G_SAVE, NUM_GLOBALS };
static const char* const GLOBAL_NAME[NUM_GLOBALS] = {"preset", "next", "prev", "on", "brightness", "link", "save"};

enum PSlot : uint8_t { P_PLAY, P_EFFECT, P_PALETTE, P_SPARKLE, P_TIMING, P_BEATS, P_REVERSE, P_PULSE, P_FIXED };
static const char* const PSLOT_NAME[P_FIXED] = {"play", "effect", "palette", "sparkle", "timing", "beats", "reverse", "pulse"};
// After a preset's parameters: its motions' settings
enum MSlot : uint8_t { M_TARGET, M_SHAPE, M_FROM, M_TO, M_SECONDS, M_BEATS, M_PHASE, MSLOTS };
static const char* const MSLOT_NAME[MSLOTS] = {"target", "shape", "from", "to", "seconds", "beats", "phase"};
static const int M_BASE = P_FIXED + MAXP;
static const int PSLOTS = M_BASE + MOTIONS * MSLOTS;
static const int CSLOTS = 1 + 2 * PAL_MAX;  // the colour count, the colours, their positions

static const int PRESET_BASE = NUM_GLOBALS;
static const int PALETTE_BASE = PRESET_BASE + MAX_PRESETS * PSLOTS;
static const int NUM_NODES = PALETTE_BASE + MAX_PALETTES * CSLOTS;
static const int WORDS = (NUM_NODES + 31) / 32;

struct NodeInfo {
    char type;         // OSC type: 'i' 'f' 'T' (bool) 'N' (trigger) 'r' (colour)
    uint8_t access;    // 1 read, 2 write, 3 both
    bool range;
    float min, max;
    const char* help;
};

static char effectList[400];  // "1 Rolling bars, 2 Searchlights, ..." for the effect's description

// A parameter's address name: its label in lower case up to any "(unit)", spaces as _
static void slug(const char* name, char* out, size_t len) {
    size_t n = 0;
    for (const char* s = name; *s && *s != '(' && n + 1 < len; s++) {
        const char c = *s;
        if (isalnum((unsigned char)c)) out[n++] = tolower((unsigned char)c);
        else if (c == ' ' && n && out[n - 1] != '_') out[n++] = '_';
    }
    while (n && out[n - 1] == '_') n--;
    out[n] = 0;
}

static bool paramPath(int preset, int effect, int j, char* buf, size_t len) {
    if (effect >= NUM_EFFECTS || j >= EFFECTS[effect].n) return false;
    char s[32];
    slug(EFFECTS[effect].params[j].name, s, sizeof(s));
    snprintf(buf, len, "/beamled/presets/%d/%s", preset + 1, s);
    return true;
}

// The node's address and description; false when it doesn't exist (now)
static bool nodeInfo(int id, NodeInfo& n, char* path, size_t len) {
    n = {'f', 3, false, 0, 1, ""};
    if (id < 0 || id >= NUM_NODES) return false;
    if (id < PRESET_BASE) {
        snprintf(path, len, "/beamled/%s", GLOBAL_NAME[id]);
        switch (id) {
            case G_PRESET:
                n = {'i', 3, true, 1, (float)presetCount, "The preset playing, by its number on the web page. Fades over to it, and switches on."};
                break;
            case G_NEXT: n = {'N', 2, false, 0, 0, "The next preset, as a short press of the button (after the last comes the first). An argument of 0, a button's release, is ignored."}; break;
            case G_PREV: n = {'N', 2, false, 0, 0, "The previous preset. An argument of 0 is ignored."}; break;
            case G_ON: n = {'T', 3, false, 0, 1, "On or off, with a fade, as a long press of the button. No argument toggles."}; break;
            case G_BRIGHTNESS: n = {'f', 3, true, 0, 1, "Master brightness, 0..1, as the web page's Brightness slider."}; break;
            case G_LINK: n = {'T', 3, false, 0, 1, "Ableton Link: Enabled (true) or Disabled (false). No argument toggles."}; break;
            case G_SAVE: n = {'N', 2, false, 0, 0, "Saves every preset as it plays now (as Save on each preset on the web page), with the palettes and settings, so they survive a power cut. Until then, preset changes from OSC or the web page play but are not saved."}; break;
        }
        return true;
    }
    if (id < PALETTE_BASE) {
        const int i = (id - PRESET_BASE) / PSLOTS, slot = (id - PRESET_BASE) % PSLOTS;
        if (i >= presetCount) return false;
        const Preset& p = presets[i];
        if (slot >= M_BASE) {
            const int m = (slot - M_BASE) / MSLOTS, k = (slot - M_BASE) % MSLOTS;
            const Motion& mo = presetMotion[i][m];
            const EffectDef& ef = EFFECTS[p.effect];
            snprintf(path, len, "/beamled/presets/%d/motion%d_%s", i + 1, m + 1, MSLOT_NAME[k]);
            float lo = 0, hi = 1;
            if (mo.target && mo.target <= ef.n) lo = ef.params[mo.target - 1].min, hi = ef.params[mo.target - 1].max;
            switch (k) {
                case M_TARGET: n = {'i', 3, true, 0, (float)ef.n, "Motion: the slider it moves back and forth, by its place in the list on the web page: 0 = off, 1 = the first slider, 2 = the second, and so on."}; break;
                case M_SHAPE: n = {'i', 3, true, 0, NUM_SHAPES - 1, "Motion: how it moves between the ends. 0 = sine (smooth, like a pendulum), 1 = linear (steady, turning sharply), 2 = ease in-out (slow at the ends), 3 = hold (rests at each end), 4 = overshoot (a little past each end), 5 = bounce."}; break;
                case M_FROM: n = {'f', 3, true, lo, hi, "Motion: one end, in the slider's own units."}; break;
                case M_TO: n = {'f', 3, true, lo, hi, "Motion: the other end, in the slider's own units."}; break;
                case M_SECONDS: n = {'f', 3, true, 0.25f, 120, "Motion: one full there-and-back, in seconds (when beats is 0). Following the tempo when the preset does."}; break;
                case M_BEATS: n = {'i', 3, true, 0, 64, "Motion: 0 = its period is in seconds; 1, 2, 4 ... 64 = one there-and-back takes that many beats, on the beat grid (at phase 0 it is at From on the bar)."}; break;
                case M_PHASE: n = {'f', 3, true, 0, 1, "Motion: where in its cycle it is, 0..1: 0 = at From, 0.5 = at To. Two motions out of step, say."}; break;
            }
            return true;
        }
        if (slot >= P_FIXED) {
            const int j = slot - P_FIXED;
            if (!paramPath(i, p.effect, j, path, len)) return false;
            const ParamDef& d = EFFECTS[p.effect].params[j];
            n = {d.step >= 1 ? 'i' : 'f', 3, true, d.min, d.max, d.help};
            return true;
        }
        snprintf(path, len, "/beamled/presets/%d/%s", i + 1, PSLOT_NAME[slot]);
        switch (slot) {
            case P_PLAY: n = {'T', 3, false, 0, 1, "True while this preset is the one playing (or being faded to). Send true to play it; false is ignored."}; break;
            case P_EFFECT: n = {'i', 3, true, 1, (float)NUM_EFFECTS, effectList}; break;
            case P_PALETTE: n = {'i', 3, true, 1, (float)paletteCount, "The palette, by its number in the Palettes list on the web page (1 = the first)."}; break;
            case P_SPARKLE: n = {'f', 3, true, 0, 1, "Sparkle overlay: white twinkles on top of the effect. 0 = none, 1 = about 8% of the LEDs lit at a time."}; break;
            case P_TIMING: n = {'i', 3, true, 0, 2, "0 = time (speeds in their own units), 1 = follow the tempo (the set speed at 120 BPM, scaled with the tempo), 2 = beats (the effect's cycle takes a whole number of beats and lands on the bar; only for effects with a cycle)."}; break;
            case P_BEATS: n = {'i', 3, true, 1, 64, "With timing 2 (beats): how many beats one cycle takes: 1, 2, 4, 8, 16, 32 or 64 (other numbers go to the nearest of these)."}; break;
            case P_REVERSE: n = {'T', 3, false, 0, 1, "With timing 2 (beats): run the cycle backwards. No argument toggles."}; break;
            case P_PULSE: n = {'f', 3, true, 0, 1, "Beat pulse: brightness dips between beats and is full on each beat, strongest on the bar's first beat. 0 = none."}; break;
        }
        return true;
    }
    const int k = (id - PALETTE_BASE) / CSLOTS, slot = (id - PALETTE_BASE) % CSLOTS;
    if (k >= paletteCount) return false;
    if (slot > PAL_MAX) {
        snprintf(path, len, "/beamled/palettes/%d/position%d", k + 1, slot - PAL_MAX);
        n = {'f', 3, true, 0, 1, "Where this colour sits along the palette, 0..1 (0 = the start, 1 = the end). The colours stay in order: a position is kept between its neighbours'."};
        return true;
    }
    if (slot == 0) {
        snprintf(path, len, "/beamled/palettes/%d/colours", k + 1);
        n = {'i', 3, true, 2, PAL_MAX, "How many colours the palette uses, 2..5. Colours beyond the count keep their value for when it grows again."};
    } else {
        snprintf(path, len, "/beamled/palettes/%d/colour%d", k + 1, slot);
        n = {'r', 3, false, 0, 0, "A colour of the palette (RGBA; alpha is ignored). Also takes \"#RRGGBB\" or three numbers (0..1, or 0..255). Setting a colour beyond the count adds it."};
    }
    return true;
}

// The node's current value as 32 bits (float bits, int, 0/1, or 0xRRGGBBAA); false for
// triggers and nodes that don't exist
static bool nodeValue(int id, char type, uint32_t& v) {
    auto f = [&](float x) { memcpy(&v, &x, 4); return true; };
    auto i = [&](int x) { v = (uint32_t)x; return true; };
    if (type == 'N') return false;
    if (id < PRESET_BASE) {
        switch (id) {
            case G_PRESET: return i(engineCurrent() + 1);
            case G_ON: return i(engineOn());
            case G_BRIGHTNESS: return f(settings.brightness / 255.f);
            case G_LINK: return i(linkSettings.enabled != 0);
        }
        return false;
    }
    if (id < PALETTE_BASE) {
        const int k = (id - PRESET_BASE) / PSLOTS, slot = (id - PRESET_BASE) % PSLOTS;
        const Preset& p = presets[k];
        const PresetSync& y = presetSync[k];
        if (slot >= M_BASE) {
            const Motion& mo = presetMotion[k][(slot - M_BASE) / MSLOTS];
            switch ((slot - M_BASE) % MSLOTS) {
                case M_TARGET: return i(mo.target);
                case M_SHAPE: return i(mo.shape);
                case M_FROM: return f(mo.from);
                case M_TO: return f(mo.to);
                case M_SECONDS: return f(mo.seconds);
                case M_BEATS: return i(mo.inBeats ? 1 << mo.beatsExp : 0);
                case M_PHASE: return f(mo.phase);
            }
            return false;
        }
        if (slot >= P_FIXED) return type == 'i' ? i((int)lroundf(p.p[slot - P_FIXED])) : f(p.p[slot - P_FIXED]);
        switch (slot) {
            case P_PLAY: return i(engineCurrent() == k);
            case P_EFFECT: return i(p.effect + 1);
            case P_PALETTE: return i(p.palette + 1);
            case P_SPARKLE: return f(p.sparkle);
            case P_TIMING: return i(y.timing);
            case P_BEATS: return i(1 << y.beatsExp);
            case P_REVERSE: return i(y.reverse != 0);
            case P_PULSE: return f(y.pulse);
        }
        return false;
    }
    const int k = (id - PALETTE_BASE) / CSLOTS, slot = (id - PALETTE_BASE) % CSLOTS;
    const Palette& pal = palettes[k];
    if (slot == 0) return i(pal.n);
    if (slot > PAL_MAX) return f(pal.pos[slot - PAL_MAX - 1]);
    const CRGB& c = pal.c[slot - 1];
    v = (uint32_t)c.r << 24 | (uint32_t)c.g << 16 | (uint32_t)c.b << 8 | 0xff;
    return true;
}

// The node at an address, or -1
static int findNode(const char* path) {
    if (strncmp(path, "/beamled/", 9) != 0) return -1;
    const char* r = path + 9;
    for (int g = 0; g < NUM_GLOBALS; g++)
        if (!strcmp(r, GLOBAL_NAME[g])) return g;
    char* e;
    if (!strncmp(r, "presets/", 8)) {
        const long i = strtol(r + 8, &e, 10) - 1;
        if (*e != '/' || i < 0 || i >= presetCount) return -1;
        const char* name = e + 1;
        const int base = PRESET_BASE + i * PSLOTS;
        for (int s = 0; s < P_FIXED; s++)
            if (!strcmp(name, PSLOT_NAME[s])) return base + s;
        if (!strncmp(name, "motion", 6) && name[6] >= '1' && name[6] < '1' + MOTIONS && name[7] == '_')
            for (int k = 0; k < MSLOTS; k++)
                if (!strcmp(name + 8, MSLOT_NAME[k])) return base + M_BASE + (name[6] - '1') * MSLOTS + k;
        const EffectDef& ef = EFFECTS[presets[i].effect];
        for (int j = 0; j < ef.n; j++) {
            char s[32];
            slug(ef.params[j].name, s, sizeof(s));
            if (!strcmp(name, s)) return base + P_FIXED + j;
        }
        return -1;
    }
    if (!strncmp(r, "palettes/", 9)) {
        const long k = strtol(r + 9, &e, 10) - 1;
        if (*e != '/' || k < 0 || k >= paletteCount) return -1;
        const int base = PALETTE_BASE + k * CSLOTS;
        if (!strcmp(e + 1, "colours")) return base;
        if (!strncmp(e + 1, "position", 8)) {
            char* e2;
            const long j = strtol(e + 9, &e2, 10);
            if (*e2 == 0 && j >= 1 && j <= PAL_MAX) return base + PAL_MAX + j;
        }
        if (!strncmp(e + 1, "colour", 6)) {
            char* e2;
            const long j = strtol(e + 7, &e2, 10);
            if (*e2 == 0 && j >= 1 && j <= PAL_MAX) return base + j;
        }
    }
    return -1;
}

// ---------------------------------------------------------------- OSC messages

struct Arg {
    char t;  // 'i' 'f' 's' 'T' 'F' 'r'
    int32_t i;
    float f;
    uint32_t r;
    const char* s;
};

static bool argNumber(const Arg& a, float& out) {
    switch (a.t) {
        case 'i': out = a.i; return true;
        case 'f': out = a.f; return true;
        case 'T': out = 1; return true;
        case 'F': out = 0; return true;
        case 's': {
            char* e;
            out = strtof(a.s, &e);
            return e != a.s && *e == 0;
        }
    }
    return false;
}

static bool argColour(const Arg* a, int n, CRGB& out) {
    if (n >= 1 && a[0].t == 'r') {
        out = CRGB(a[0].r >> 24, a[0].r >> 16, a[0].r >> 8);
        return true;
    }
    if (n >= 1 && a[0].t == 's' && a[0].s[0] == '#' && (strlen(a[0].s) == 7 || strlen(a[0].s) == 9)) {
        const uint32_t v = strtoul(a[0].s + 1, nullptr, 16);
        out = strlen(a[0].s) == 9 ? CRGB(v >> 24, v >> 16, v >> 8) : CRGB(v);
        return true;
    }
    float c[3];
    if (n >= 3 && argNumber(a[0], c[0]) && argNumber(a[1], c[1]) && argNumber(a[2], c[2])) {
        const bool unit = a[0].t == 'f' && c[0] <= 1 && c[1] <= 1 && c[2] <= 1;
        uint8_t b[3];
        for (int k = 0; k < 3; k++) b[k] = (uint8_t)constrain(lroundf(unit ? c[k] * 255.f : c[k]), 0L, 255L);
        out = CRGB(b[0], b[1], b[2]);
        return true;
    }
    return false;
}

static String lastMessage;
static uint32_t received = 0;

// One message from the network, applied
static void apply(const char* path, const Arg* a, int n) {
    received++;
    lastMessage = path;
    const int id = findNode(path);
    char buf[64];
    NodeInfo info;
    if (id < 0 || !nodeInfo(id, info, buf, sizeof(buf))) {
        lastMessage += " (unknown)";
        return;
    }
    float v = 0;
    const bool has = n > 0 && argNumber(a[0], v);
    if (info.type == 'N' && has && v == 0) return;  // a button's release
    if (info.type == 'T' && !has && n == 0) {        // no argument: toggle
        uint32_t cur = 0;
        nodeValue(id, 'T', cur);
        v = cur ? 0 : 1;
    } else if (info.type != 'N' && info.type != 'r' && !has) {
        lastMessage += " (no value)";
        return;
    }
    if (info.type == 'f' || info.type == 'i') {
        if (!isfinite(v)) return;
        if (info.range && id >= PRESET_BASE) v = constrain(v, info.min, info.max);
        if (info.type == 'i') v = roundf(v);
    }
    const bool on = v >= 0.5f;

    if (id < PRESET_BASE) {
        const int cur = engineCurrent();
        switch (id) {
            case G_PRESET: engineSelect((int)v - 1); break;
            case G_NEXT: engineSelect((cur + 1) % presetCount); break;
            case G_PREV: engineSelect((cur + presetCount - 1) % presetCount); break;
            case G_ON: engineSetOn(on); break;
            case G_BRIGHTNESS: settings.brightness = (uint8_t)lroundf(constrain(v, 0.f, 1.f) * 255.f); break;
            case G_LINK: linkSettings.enabled = on; break;
            case G_SAVE: presetSaveAll(); storeMarkDirty(); break;
        }
        return;
    }
    if (id < PALETTE_BASE) {
        const int i = (id - PRESET_BASE) / PSLOTS, slot = (id - PRESET_BASE) % PSLOTS;
        Preset& p = presets[i];
        PresetSync& y = presetSync[i];
        if (slot >= M_BASE) {
            Motion& mo = presetMotion[i][(slot - M_BASE) / MSLOTS];
            const EffectDef& ef = EFFECTS[p.effect];
            switch ((slot - M_BASE) % MSLOTS) {
                case M_TARGET:
                    if ((int)v != mo.target) {
                        mo.target = (int)v;
                        if (mo.target) {  // a new slider: its whole range to start with
                            mo.from = ef.params[mo.target - 1].min;
                            mo.to = ef.params[mo.target - 1].max;
                        }
                        webChanged();  // the page shows the motion's settings
                    }
                    break;
                case M_SHAPE: mo.shape = (int)v; break;
                case M_FROM: mo.from = v; break;
                case M_TO: mo.to = v; break;
                case M_SECONDS: mo.seconds = v; break;
                case M_BEATS:
                    mo.inBeats = v >= 0.5f;
                    if (mo.inBeats) mo.beatsExp = constrain((int)lroundf(log2f(v)), 0, 6);
                    break;
                case M_PHASE: mo.phase = v; break;
            }
            return;
        }
        if (slot >= P_FIXED) {
            p.p[slot - P_FIXED] = v;
            return;
        }
        switch (slot) {
            case P_PLAY:
                if (on) engineSelect(i);
                break;
            case P_EFFECT:
                if ((int)v - 1 != p.effect) {
                    webChanged();  // the page shows the new effect's sliders
                    presetChangeEffect(i, (int)v - 1);
                    if (i == settings.current) engineResetEffect();
                }
                break;
            case P_PALETTE: p.palette = (int)v - 1; break;
            case P_SPARKLE: p.sparkle = v; break;
            case P_TIMING: y.timing = (int)v; break;
            case P_BEATS: y.beatsExp = constrain((int)lroundf(log2f(fmaxf(v, 1.f))), 0, 6); break;
            case P_REVERSE: y.reverse = on; break;
            case P_PULSE: y.pulse = v; break;
        }
        return;
    }
    const int k = (id - PALETTE_BASE) / CSLOTS, slot = (id - PALETTE_BASE) % CSLOTS;
    Palette& pal = palettes[k];
    if (slot == 0) {
        if (pal.n != (int)v) {
            webChanged();  // colours come or go on the page
            const bool even = paletteHasEvenStops(pal);
            pal.n = (int)v;
            if (even) paletteEvenStops(pal);
            else paletteSanitizeStops(pal);
        }
        return;
    }
    if (slot > PAL_MAX) {
        const int j = slot - PAL_MAX - 1;
        const float lo = j > 0 ? pal.pos[j - 1] : 0.f, hi = j < pal.n - 1 ? pal.pos[j + 1] : 1.f;
        pal.pos[j] = constrain(v, lo, hi);
        return;
    }
    CRGB c;
    if (!argColour(a, n, c)) {
        lastMessage += " (not a colour)";
        return;
    }
    pal.c[slot - 1] = c;
    if (slot > pal.n) {
        const bool even = paletteHasEvenStops(pal);
        pal.n = slot;
        if (even) paletteEvenStops(pal);
        else paletteSanitizeStops(pal);
    }
}

static bool readString(const uint8_t* d, size_t size, size_t& pos, const char*& out) {
    const size_t start = pos;
    while (pos < size && d[pos] != 0) pos++;
    if (pos >= size) return false;
    out = (const char*)d + start;
    pos = (pos + 4) & ~(size_t)3;
    return pos <= size;
}

static bool readU32(const uint8_t* d, size_t size, size_t& pos, uint32_t& out) {
    if (pos + 4 > size) return false;
    out = (uint32_t)d[pos] << 24 | (uint32_t)d[pos + 1] << 16 | (uint32_t)d[pos + 2] << 8 | d[pos + 3];
    pos += 4;
    return true;
}

// A message or a bundle (time tags ignored: messages apply when they arrive), as in Glow.
// The buffer must be writable and end in a 0 (strings point into it).
static void parsePacket(const uint8_t* d, size_t size, int depth = 0) {
    if (size < 4 || depth > 4) return;
    size_t pos = 0;
    const char* address;
    if (!readString(d, size, pos, address)) return;
    if (!strcmp(address, "#bundle")) {
        pos += 8;
        uint32_t len;
        while (readU32(d, size, pos, len) && pos + len <= size) {
            parsePacket(d + pos, len, depth + 1);
            pos += len;
        }
        return;
    }
    if (address[0] != '/') return;
    Arg args[4];
    int n = 0;
    const char* tags;
    if (pos < size && d[pos] == ',' && readString(d, size, pos, tags)) {
        for (const char* t = tags + 1; *t && n < 4; t++) {
            Arg& a = args[n];
            a = {*t, 0, 0, 0, ""};
            uint32_t u;
            if (*t == 'i' && readU32(d, size, pos, u)) a.i = (int32_t)u;
            else if (*t == 'f' && readU32(d, size, pos, u)) memcpy(&a.f, &u, 4);
            else if (*t == 'r' && readU32(d, size, pos, u)) a.r = u;
            else if ((*t == 's' || *t == 'S') && readString(d, size, pos, a.s)) a.t = 's';
            else if (*t == 'T' || *t == 'F') {}
            else if (*t == 'N' || *t == 'I') continue;  // no data, no argument
            else break;
            n++;
        }
    }
    apply(address, args, n);
}

// One message with one value, as sent to listeners
static size_t encode(const char* path, char type, uint32_t v, uint8_t* out, size_t cap) {
    size_t n = 0;
    auto put = [&](const char* s) {
        const size_t len = strlen(s);
        if (n + len + 4 > cap) return false;
        memcpy(out + n, s, len);
        n += len;
        do out[n++] = 0; while (n % 4);
        return true;
    };
    char tags[3] = {',', type == 'T' ? (v ? 'T' : 'F') : type, 0};
    if (!put(path) || !put(tags)) return 0;
    if (type == 'T') return n;
    if (n + 4 > cap) return 0;
    for (int s = 24; s >= 0; s -= 8) out[n++] = (uint8_t)(v >> s);
    return n;
}

// ---------------------------------------------------------------- OSCQuery: HTTP

static httpd_handle_t server = nullptr;

// A JSON reply streamed out in chunks, so the whole tree never has to be in memory. Bigger chunks
// make the tree come down much faster (each chunk is a TCP round trip).
static const size_t CHUNK = 4096;
static char* chunkBuf = nullptr;  // CHUNK bytes from the heap (static DRAM is full)
struct Out {
    httpd_req_t* req;
    char* buf;  // the shared chunk buffer: only the server's task writes replies
    size_t n = 0;
    void flush() {
        if (n) httpd_resp_send_chunk(req, buf, n);
        n = 0;
    }
    void raw(const char* s) {
        while (*s) {
            if (n == CHUNK) flush();
            buf[n++] = *s++;
        }
    }
    void str(const char* s) {
        raw("\"");
        char e[8];
        for (; *s; s++) {
            const unsigned char c = *s;
            if (c == '"' || c == '\\') { e[0] = '\\'; e[1] = c; e[2] = 0; raw(e); }
            else if (c < 0x20) { snprintf(e, sizeof(e), "\\u%04x", c); raw(e); }
            else { e[0] = c; e[1] = 0; raw(e); }
        }
        raw("\"");
    }
    void num(float v) {
        char b[24];
        snprintf(b, sizeof(b), "%.6g", v);
        raw(b);
    }
};

static void writeValue(Out& o, char type, uint32_t v) {
    if (type == 'T') return o.raw(v ? "true" : "false");
    if (type == 'i') {
        char b[16];
        snprintf(b, sizeof(b), "%d", (int)v);
        return o.raw(b);
    }
    if (type == 'r') {
        char b[12];
        snprintf(b, sizeof(b), "#%08X", (unsigned)v);
        return o.str(b);
    }
    float f;
    memcpy(&f, &v, 4);
    o.num(f);
}

// One address. attr: only that attribute ({"VALUE": [...]}), or all of them
static void writeMethod(Out& o, int id, const NodeInfo& n, const char* path, const char* attr) {
    bool first = true;
    auto key = [&](const char* k) {
        if (attr && strcmp(attr, k)) return false;
        o.raw(first ? "\"" : ",\"");
        o.raw(k);
        o.raw("\":");
        first = false;
        return true;
    };
    o.raw("{");
    if (key("FULL_PATH")) o.str(path);
    if (key("TYPE")) {
        const char t[2] = {n.type, 0};
        o.str(t);
    }
    if (key("ACCESS")) o.raw(n.access == 1 ? "1" : n.access == 2 ? "2" : "3");
    if (key("DESCRIPTION")) o.str(n.help);
    if (n.range && key("RANGE")) {
        o.raw("[{\"MIN\":");
        o.num(n.min);
        o.raw(",\"MAX\":");
        o.num(n.max);
        o.raw("}]");
    }
    uint32_t v;
    if (nodeValue(id, n.type, v) && key("VALUE")) {
        o.raw("[");
        writeValue(o, n.type, v);
        o.raw("]");
    }
    o.raw("}");
}

static void writeTree(Out& o, const char* path);

// A container: its addresses (and sub-containers), nested by path
static void writeContainer(Out& o, const char* path, const char* help, int from, int to,
                           const char* const* subs = nullptr, int nsubs = 0, const char* subFmt = nullptr) {
    o.raw("{\"FULL_PATH\":");
    o.str(path);
    o.raw(",\"ACCESS\":0,\"DESCRIPTION\":");
    o.str(help);
    o.raw(",\"CONTENTS\":{");
    bool first = true;
    char p[64];
    NodeInfo n;
    for (int id = from; id < to; id++) {
        if (!nodeInfo(id, n, p, sizeof(p))) continue;
        o.raw(first ? "" : ",");
        first = false;
        o.str(strrchr(p, '/') + 1);
        o.raw(":");
        writeMethod(o, id, n, p, nullptr);
    }
    for (int s = 0; s < nsubs; s++) {
        o.raw(first ? "" : ",");
        first = false;
        o.str(subs[s]);
        o.raw(":");
        snprintf(p, sizeof(p), subFmt, subs[s]);
        writeTree(o, p);
    }
    o.raw("}}");
}

// Container paths: /, /beamled, /beamled/presets, /beamled/presets/<n>, /beamled/palettes,
// /beamled/palettes/<k>. False when it isn't one.
static bool isContainer(const char* path) {
    if (!strcmp(path, "/") || !strcmp(path, "/beamled") || !strcmp(path, "/beamled/presets") ||
        !strcmp(path, "/beamled/palettes"))
        return true;
    char* e;
    if (!strncmp(path, "/beamled/presets/", 17)) {
        const long i = strtol(path + 17, &e, 10);
        return e != path + 17 && *e == 0 && i >= 1 && i <= presetCount;
    }
    if (!strncmp(path, "/beamled/palettes/", 18)) {
        const long k = strtol(path + 18, &e, 10);
        return e != path + 18 && *e == 0 && k >= 1 && k <= paletteCount;
    }
    return false;
}

static void writeTree(Out& o, const char* path) {
    static char nums[MAX_PRESETS][4];
    static const char* numPtrs[MAX_PRESETS];
    for (int i = 0; i < MAX_PRESETS; i++) {
        snprintf(nums[i], sizeof(nums[i]), "%d", i + 1);
        numPtrs[i] = nums[i];
    }
    char help[64];
    if (!strcmp(path, "/")) {
        static const char* const top[] = {"beamled"};
        writeContainer(o, "/", "beamLED", 0, 0, top, 1, "/%s");
    } else if (!strcmp(path, "/beamled")) {
        static const char* const subs[] = {"presets", "palettes"};
        writeContainer(o, path, "beamLED: the LED ceiling", 0, NUM_GLOBALS, subs, 2, "/beamled/%s");
    } else if (!strcmp(path, "/beamled/presets")) {
        writeContainer(o, path, "The presets, numbered as on the web page", 0, 0, numPtrs, presetCount, "/beamled/presets/%s");
    } else if (!strcmp(path, "/beamled/palettes")) {
        writeContainer(o, path, "The palettes, numbered as on the web page", 0, 0, numPtrs, paletteCount, "/beamled/palettes/%s");
    } else if (!strncmp(path, "/beamled/presets/", 17)) {
        const int i = atoi(path + 17) - 1;
        snprintf(help, sizeof(help), "%s: %s", presets[i].name, EFFECTS[presets[i].effect].name);
        const int base = PRESET_BASE + i * PSLOTS;
        writeContainer(o, path, help, base, base + PSLOTS);
    } else {
        const int k = atoi(path + 18) - 1;
        snprintf(help, sizeof(help), "Palette %s", palettes[k].name);
        const int base = PALETTE_BASE + k * CSLOTS;
        writeContainer(o, path, help, base, base + CSLOTS);
    }
}

// A browser (rather than an OSCQuery client) asking: it accepts HTML
static bool wantsHtml(httpd_req_t* req) {
    char a[160] = "";
    httpd_req_get_hdr_value_str(req, "Accept", a, sizeof(a));
    return strstr(a, "text/html") != nullptr;
}

static esp_err_t serveHttp(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    char path[96];
    strlcpy(path, req->uri, sizeof(path));
    char* q = strchr(path, '?');
    const char* query = "";
    if (q) {
        *q = 0;
        query = q + 1;
    }
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') path[--len] = 0;

    if (!strcmp(query, "HOST_INFO")) {
        char b[512];
        String name = netSettings.name;
        name.replace("\"", "'");
        snprintf(b, sizeof(b),
                 "{\"NAME\":\"%s\",\"OSC_PORT\":%u,\"OSC_TRANSPORT\":\"UDP\",\"EXTENSIONS\":{\"ACCESS\":true,"
                 "\"VALUE\":true,\"RANGE\":true,\"DESCRIPTION\":true,\"TAGS\":false,\"EXTENDED_TYPE\":false,"
                 "\"UNIT\":false,\"CRITICAL\":false,\"CLIPMODE\":false,\"LISTEN\":true,\"PATH_CHANGED\":false,"
                 "\"PATH_ADDED\":true,\"PATH_REMOVED\":true}}",
                 name.c_str(), cfg::OSC_PORT);
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, b, HTTPD_RESP_USE_STRLEN);
    }
    static const char* const ATTRS[] = {"VALUE", "TYPE", "ACCESS", "RANGE", "DESCRIPTION", "FULL_PATH", "CONTENTS"};
    const char* attr = nullptr;
    for (const char* a : ATTRS)
        if (!strcmp(query, a)) attr = a;

    Out o{req, chunkBuf};
    const int id = findNode(path);
    NodeInfo n;
    char p[64];
    if (id >= 0 && nodeInfo(id, n, p, sizeof(p))) {
        uint32_t v;
        const bool has = !attr || (!strcmp(attr, "VALUE") ? nodeValue(id, n.type, v)
                                  : !strcmp(attr, "RANGE") ? n.range
                                  : strcmp(attr, "CONTENTS") != 0);
        if (!has) {
            httpd_resp_set_status(req, "204 No Content");
            return httpd_resp_send(req, nullptr, 0);
        }
        httpd_resp_set_type(req, "application/json");
        writeMethod(o, id, n, p, attr);
    } else if (isContainer(path)) {
        httpd_resp_set_type(req, "application/json");
        writeTree(o, path);  // attributes of a container: the container itself
    } else {
        if (wantsHtml(req)) return webRedirect(req);  // a browser: to the page
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_send(req, "no such address", HTTPD_RESP_USE_STRLEN);
    }
    o.flush();
    return httpd_resp_send_chunk(req, nullptr, 0);
}

// ---------------------------------------------------------------- OSCQuery: LISTEN
// Everything below runs on the server's own task (handlers, close, and the queued push), so
// the client list needs no lock.

static const int MAX_LISTENERS = 4;  // MIMa and a few web pages
struct Listener {
    int fd = -1;
    bool page = false;  // the web page: also gets the status, the change counter and scan results
    char pageId[24] = "";  // which browser tab (it reconnects under the same id)
    uint32_t bits[WORDS];
};
static Listener listeners[MAX_LISTENERS];
static volatile int listenerCount = 0;
static volatile bool pushQueued = false;

static uint32_t* lastValue = nullptr;  // NUM_NODES, from the heap (static DRAM is full)
static uint32_t known[WORDS];  // lastValue holds what listeners were last sent
static uint8_t treeEffect[MAX_PRESETS];
static int treeCount = -1;     // the presets listeners know about (-1: none yet)
static int treePalettes = -1;  // and the palettes

static inline bool hasBit(const uint32_t* b, int id) { return b[id >> 5] >> (id & 31) & 1; }
static inline void setBit(uint32_t* b, int id, bool on) {
    if (on) b[id >> 5] |= 1u << (id & 31);
    else b[id >> 5] &= ~(1u << (id & 31));
}

static void sendFrame(int fd, httpd_ws_type_t type, const uint8_t* data, size_t len) {
    httpd_ws_frame_t f = {};
    f.final = true;
    f.type = type;
    f.payload = (uint8_t*)data;
    f.len = len;
    httpd_ws_send_frame_async(server, fd, &f);
}

// A new subscription gets no value straight away: only changes from now on. MIMa's Learn takes
// the first message after its subscriptions as the one to learn, and the board can't answer 200
// subscriptions within the 0.5 s it ignores; clients that want the current value ask ?VALUE.
// The value as it is now counts as sent, so the next push doesn't send it either.
static void markCurrent(int id) {
    NodeInfo n;
    char p[64];
    uint32_t v;
    if (hasBit(known, id) || !nodeInfo(id, n, p, sizeof(p)) || !nodeValue(id, n.type, v)) return;
    lastValue[id] = v;
    setBit(known, id, true);
}
static void sendStatus(int fd) {
    const String s = "{\"COMMAND\":\"STATUS\",\"DATA\":" + webStatusJson() + "}";
    sendFrame(fd, HTTPD_WS_TYPE_TEXT, (const uint8_t*)s.c_str(), s.length());
}

static void notifyPath(const char* command, const char* path) {
    char b[128];
    snprintf(b, sizeof(b), "{\"COMMAND\":\"%s\",\"DATA\":\"%s\"}", command, path);
    for (Listener& l : listeners)
        if (l.fd >= 0) sendFrame(l.fd, HTTPD_WS_TYPE_TEXT, (const uint8_t*)b, strlen(b));
}

// Forget values and subscriptions for nodes [from, to): their address now means something else
static void forget(int from, int to) {
    for (int id = from; id < to; id++) {
        setBit(known, id, false);
        for (Listener& l : listeners) setBit(l.bits, id, false);
    }
}

// Tells listeners about presets added, removed, or given another effect (other parameters)
static void updateTree() {
    if (treePalettes < 0) treePalettes = paletteCount;
    if (treePalettes != paletteCount) {
        char q[48];
        for (int k = 0; k < max(treePalettes, paletteCount); k++) {
            const bool was = k < treePalettes, is = k < paletteCount;
            if (was == is) continue;
            snprintf(q, sizeof(q), "/beamled/palettes/%d", k + 1);
            notifyPath(is ? "PATH_ADDED" : "PATH_REMOVED", q);
            forget(PALETTE_BASE + k * CSLOTS, PALETTE_BASE + (k + 1) * CSLOTS);
        }
        treePalettes = paletteCount;
    }
    if (treeCount < 0) {
        treeCount = presetCount;
        for (int i = 0; i < MAX_PRESETS; i++) treeEffect[i] = presets[i].effect;
        return;
    }
    char p[64];
    const int n = max(treeCount, presetCount);
    for (int i = 0; i < n; i++) {
        const bool was = i < treeCount, is = i < presetCount;
        const int base = PRESET_BASE + i * PSLOTS;
        if (was && !is) {
            snprintf(p, sizeof(p), "/beamled/presets/%d", i + 1);
            notifyPath("PATH_REMOVED", p);
            forget(base, base + PSLOTS);
        } else if (!was && is) {
            snprintf(p, sizeof(p), "/beamled/presets/%d", i + 1);
            notifyPath("PATH_ADDED", p);
            forget(base, base + PSLOTS);
        } else if (is && presets[i].effect != treeEffect[i]) {
            for (int j = 0; j < MAXP; j++)
                if (paramPath(i, treeEffect[i], j, p, sizeof(p))) notifyPath("PATH_REMOVED", p);
            for (int j = 0; j < MAXP; j++)
                if (paramPath(i, presets[i].effect, j, p, sizeof(p))) notifyPath("PATH_ADDED", p);
            forget(base + P_FIXED, base + M_BASE);
        }
        if (is) treeEffect[i] = presets[i].effect;
    }
    treeCount = presetCount;
}

// Sends every change since last time to the listeners that follow it
// Changes go out as OSC bundles, one WebSocket frame per listener for everything that changed
// since the last push (split when a bundle fills a packet): each frame costs a network round
// trip on the board, each extra message in it almost nothing.
static const size_t BUNDLE_MAX = 1400;
static uint8_t (*bundle)[BUNDLE_MAX] = nullptr;  // one per listener, from the heap
static size_t bundleLen[MAX_LISTENERS];

static void bundleFlush(int k) {
    if (bundleLen[k] > 16 && listeners[k].fd >= 0) sendFrame(listeners[k].fd, HTTPD_WS_TYPE_BINARY, bundle[k], bundleLen[k]);
    // "#bundle", then the time tag 1 = at once
    static const uint8_t head[16] = {'#', 'b', 'u', 'n', 'd', 'l', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 1};
    memcpy(bundle[k], head, 16);
    bundleLen[k] = 16;
}

static void bundleAdd(int k, const uint8_t* m, size_t len) {
    if (bundleLen[k] + 4 + len > BUNDLE_MAX) bundleFlush(k);
    uint8_t* b = bundle[k] + bundleLen[k];
    b[0] = len >> 24; b[1] = len >> 16; b[2] = len >> 8; b[3] = len;
    memcpy(b + 4, m, len);
    bundleLen[k] += 4 + len;
}

static void push(void*) {
    pushQueued = false;
    if (!listenerCount) return;
    updateTree();
    for (int k = 0; k < MAX_LISTENERS; k++) bundleLen[k] = 16, bundleFlush(k);  // fresh, empty
    NodeInfo n;
    char p[64];
    for (int id = 0; id < NUM_NODES; id++) {
        uint32_t v;
        if (!nodeInfo(id, n, p, sizeof(p)) || !nodeValue(id, n.type, v)) continue;
        if (hasBit(known, id) && lastValue[id] == v) continue;
        lastValue[id] = v;
        setBit(known, id, true);
        uint8_t m[96];
        size_t len = 0;
        for (int k = 0; k < MAX_LISTENERS; k++) {
            if (listeners[k].fd < 0 || !hasBit(listeners[k].bits, id)) continue;
            if (!len) len = encode(p, n.type, v, m, sizeof(m));
            if (len) bundleAdd(k, m, len);
        }
    }
    for (int k = 0; k < MAX_LISTENERS; k++) bundleFlush(k);

    // The web page: the status once a second, scan results as soon as a scan ends
    static uint32_t lastStatus = 0;
    static int lastScan = 0;
    const int scan = netScanState();
    const bool scanDone = scan != lastScan && (scan == 2 || scan == 3);
    lastScan = scan;
    const bool status = millis() - lastStatus >= 1000;
    if (status) lastStatus = millis();
    if (!status && !scanDone) return;
    String scanText;
    if (scanDone) scanText = "{\"COMMAND\":\"SCAN\",\"DATA\":{\"state\":" + String(scan) + ",\"nets\":" + netScanResults() + "}}";
    for (Listener& l : listeners) {
        if (l.fd < 0 || !l.page) continue;
        if (status) sendStatus(l.fd);
        if (scanDone) sendFrame(l.fd, HTTPD_WS_TYPE_TEXT, (const uint8_t*)scanText.c_str(), scanText.length());
    }
}

static Listener* listenerFor(int fd) {
    for (Listener& l : listeners)
        if (l.fd == fd) return &l;
    return nullptr;
}

// LISTEN / IGNORE an address, or every address under a container
static void command(int fd, const char* text) {
    JsonDocument d;
    if (deserializeJson(d, text)) return;
    const char* cmd = d["COMMAND"] | "";
    const char* path = d["DATA"] | "";
    Listener* l = listenerFor(fd);
    if (!l) return;
    // beamLED's own web page. One page at a time: another one gets BUSY (it asks "continue here?")
    // unless it says TAKE, and then the page that had it gets TAKEN. The same tab reconnecting
    // (same ID) simply takes its place again.
    if (!strcmp(cmd, "PAGE")) {
        const char* id = d["ID"] | "";
        const bool take = d["TAKE"] | false;
        for (Listener& o : listeners) {
            if (o.fd < 0 || &o == l || !o.page) continue;
            if (!take && strcmp(o.pageId, id) != 0) {
                static const char busy[] = "{\"COMMAND\":\"BUSY\"}";
                sendFrame(fd, HTTPD_WS_TYPE_TEXT, (const uint8_t*)busy, sizeof(busy) - 1);
                return;
            }
            static const char taken[] = "{\"COMMAND\":\"TAKEN\"}";
            sendFrame(o.fd, HTTPD_WS_TYPE_TEXT, (const uint8_t*)taken, sizeof(taken) - 1);
            o.page = false;
            memset(o.bits, 0, sizeof(o.bits));
            httpd_sess_trigger_close(server, o.fd);
        }
        l->page = true;
        strlcpy(l->pageId, id, sizeof(l->pageId));
        sendStatus(fd);
        return;
    }
    const bool listen = !strcmp(cmd, "LISTEN");
    if (!listen && strcmp(cmd, "IGNORE")) return;
    updateTree();  // the subscription is to the addresses as they are now
    const int id = findNode(path);
    if (id >= 0) {
        setBit(l->bits, id, listen);
        if (listen) markCurrent(id);
        return;
    }
    if (!isContainer(path)) return;
    const size_t plen = strcmp(path, "/") ? strlen(path) : 0;
    NodeInfo n;
    char p[64];
    for (int k = 0; k < NUM_NODES; k++) {
        if (!nodeInfo(k, n, p, sizeof(p)) || strncmp(p, path, plen) || p[plen] != '/') continue;
        setBit(l->bits, k, listen);
        if (listen) markCurrent(k);
    }
}

struct WsPacket {
    uint16_t len;
    uint8_t data[256];
};
static QueueHandle_t wsIn = nullptr;  // OSC packets from WebSocket clients, for the render loop

static esp_err_t onRequest(httpd_req_t* req) {
    const int fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET) {
        // The handshake of a new WebSocket comes here as a GET too
        if (httpd_ws_get_fd_info(req->handle, fd) == HTTPD_WS_CLIENT_WEBSOCKET ||
            httpd_req_get_hdr_value_len(req, "Upgrade") > 0) {
            if (!listenerFor(fd)) {
                Listener* slot = listenerFor(-1);
                if (!slot) return ESP_FAIL;  // full: the connection is closed
                slot->fd = fd;
                slot->page = false;
                slot->pageId[0] = 0;
                memset(slot->bits, 0, sizeof(slot->bits));
                listenerCount = listenerCount + 1;
            }
            return ESP_OK;
        }
        // The web page and its API share this server (port 80) with OSCQuery
        esp_err_t r;
        if (!strncmp(req->uri, "/api/", 5) && webApi(req, r)) return r;
        if ((!strcmp(req->uri, "/") || !strcmp(req->uri, "/index.html")) && wantsHtml(req)) return webPage(req);
        return serveHttp(req);
    }
    httpd_ws_frame_t f = {};
    if (httpd_ws_recv_frame(req, &f, 0) != ESP_OK) return ESP_FAIL;
    if (f.len > 512) return ESP_FAIL;
    char text[513];
    f.payload = (uint8_t*)text;
    if (f.len && httpd_ws_recv_frame(req, &f, f.len) != ESP_OK) return ESP_FAIL;
    text[f.len] = 0;
    if (f.type == HTTPD_WS_TYPE_TEXT) command(fd, text);
    // OSC over the WebSocket (the web page's sliders): to the render loop, which applies it as
    // OSC from UDP, so nothing changes while a frame is drawn
    else if (f.type == HTTPD_WS_TYPE_BINARY && f.len && f.len < sizeof(WsPacket::data) && wsIn) {
        WsPacket pk;
        pk.len = f.len;
        memcpy(pk.data, text, f.len);
        xQueueSend(wsIn, &pk, 0);  // full: dropped (the next value follows within a frame)
    }
    return ESP_OK;
}

// Every connection sends at once: without this, Nagle holds each small reply until the last
// one is acknowledged, and Windows delays its acknowledgements, so a value takes milliseconds
// and the tree seconds. MIMa's Learn ignores values only in the first 0.5 s after it subscribes.
static esp_err_t onOpen(httpd_handle_t, int fd) {
    const int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return ESP_OK;
}

static esp_err_t onPost(httpd_req_t* req) {
    esp_err_t r;
    if (!strncmp(req->uri, "/api/", 5) && webApi(req, r)) return r;
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "no such address", HTTPD_RESP_USE_STRLEN);
}

static void onClose(httpd_handle_t, int fd) {
    if (Listener* l = listenerFor(fd)) {
        l->fd = -1;
        listenerCount = listenerCount - 1;
    }
    close(fd);
}

// ---------------------------------------------------------------- start, loop

static bool started = false;
static int udpSocket = -1;
static String startProblem;

static void start() {
    started = true;
    wsIn = xQueueCreate(16, sizeof(WsPacket));
    size_t n = 0;
    for (int e = 0; e < NUM_EFFECTS && n + 40 < sizeof(effectList); e++)
        n += snprintf(effectList + n, sizeof(effectList) - n, "%s%d %s", e ? ", " : "The effect: ", e + 1, EFFECTS[e].name);

    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = htons(cfg::OSC_PORT);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (udpSocket < 0 || bind(udpSocket, (sockaddr*)&a, sizeof(a)) < 0) {
        startProblem = "OSC port " + String(cfg::OSC_PORT) + " unavailable";
        if (udpSocket >= 0) close(udpSocket);
        udpSocket = -1;
    } else {
        fcntl(udpSocket, F_SETFL, O_NONBLOCK);
    }

    chunkBuf = (char*)malloc(CHUNK);
    lastValue = (uint32_t*)calloc(NUM_NODES, sizeof(uint32_t));
    bundle = (uint8_t (*)[BUNDLE_MAX])malloc(MAX_LISTENERS * BUNDLE_MAX);
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.server_port = cfg::OSCQUERY_PORT;
    c.ctrl_port = 32770;
    // Below what lwIP has left (16 sockets in all: Link 3, this server's own 2, OSC/Art-Net 1, OTA 1,
    // the hotspot's DNS or the DHCP probe 1): with 5, a new connection closes the one idle longest
    // (a browser's unused spare), instead of lwIP refusing it
    c.max_open_sockets = 5;
    c.lru_purge_enable = true;
    c.uri_match_fn = httpd_uri_match_wildcard;
    c.open_fn = onOpen;
    c.close_fn = onClose;
    c.core_id = 0;  // with the network stack; the render loop has core 1
    c.stack_size = 8192;  // the web API's JSON too
    if (chunkBuf && bundle && lastValue && httpd_start(&server, &c) == ESP_OK) {
        httpd_uri_t u = {};
        u.uri = "/*";
        u.method = HTTP_GET;
        u.handler = onRequest;
        u.is_websocket = true;
        httpd_register_uri_handler(server, &u);
        httpd_uri_t post = {};
        post.uri = "/*";
        post.method = HTTP_POST;
        post.handler = onPost;
        httpd_register_uri_handler(server, &post);
        MDNS.addService("oscjson", "tcp", cfg::OSCQUERY_PORT);
    } else {
        server = nullptr;
        startProblem += (startProblem.length() ? ", " : "") + String("OSCQuery could not start");
    }
    if (udpSocket >= 0) MDNS.addService("osc", "udp", cfg::OSC_PORT);
    Serial.printf("OSC in on UDP %u (socket %d), OSCQuery on port %u%s\n", cfg::OSC_PORT, udpSocket,
                  cfg::OSCQUERY_PORT, server ? "" : " FAILED");
}

void oscLoop() {
    if (!started) {
        if (!netCanSend()) return;
        start();
    }
    EngineLock lock;  // the web API changes the same things from the HTTP server's task
    // OSC that came over the WebSocket (the page's sliders)
    if (wsIn) {
        static WsPacket pk;
        while (xQueueReceive(wsIn, &pk, 0) == pdTRUE) {
            pk.data[pk.len < sizeof(pk.data) ? pk.len : sizeof(pk.data) - 1] = 0;
            parsePacket(pk.data, pk.len);
        }
    }
    if (udpSocket >= 0) {
        static uint8_t buf[1024];
        for (int k = 0; k < 16; k++) {  // a burst at most per pass, so the frame isn't held up
            const int len = recv(udpSocket, buf, sizeof(buf) - 1, 0);
            if (len <= 0) break;
            buf[len] = 0;
            parsePacket(buf, len);
        }
    }
    // Feedback 20 times a second, on the server's task
    static uint32_t lastPush = 0;
    if (server && listenerCount > 0 && !pushQueued && millis() - lastPush >= 50) {
        lastPush = millis();
        pushQueued = true;
        if (httpd_queue_work(server, push, nullptr) != ESP_OK) pushQueued = false;
    }
}

int oscUdpSocket() { return udpSocket; }

String oscStatusText() {
    if (!started) return "OSC: waiting for the network";
    String s = "OSC in: UDP " + String(cfg::OSC_PORT) + " · OSCQuery: port " + String(cfg::OSCQUERY_PORT) + ", " +
               String(listenerCount) + " listening";
    if (startProblem.length()) s += " · " + startProblem;
    if (received) s += " · " + String(received) + " received, last: " + lastMessage;
    return s;
}
