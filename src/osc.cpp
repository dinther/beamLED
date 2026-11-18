#include "osc.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <esp_http_server.h>
#include <fcntl.h>
#include <lwip/sockets.h>
#include <math.h>

#include <atomic>

#include "config.h"
#include "effects.h"
#include "engine.h"
#include "link_clock.h"
#include "net.h"
#include "store.h"
#include "web.h"

// ---------------------------------------------------------------- the address space
// Every address has a fixed number (node id), so feedback can remember values and subscriptions
// in small arrays: the global controls, then slider1..slider8, the playing preset's sliders. Kept
// small on purpose (2026-11-19): per-preset settings, motions and palettes are the web page's.

enum Global : uint8_t { G_PRESET, G_NEXT, G_PREV, G_ON, G_BRIGHTNESS, G_LINK, G_SAVE, G_AUTO,
                        G_SPARKLE, G_SPARKLE_TIMING, G_SPARKLE_BURST, G_SPARKLE_COLOUR, G_SPARKLE_STEP, G_PALETTE, G_SPARKLE_THROUGH, G_SPARKLE_TIME, G_EFFECT_BRIGHTNESS, G_SPARKLE_BRIGHTNESS, NUM_GLOBALS };
static const char* const GLOBAL_NAME[NUM_GLOBALS] = {"preset", "next", "prev", "on", "brightness", "link", "save", "auto",
                                                     "sparkle", "sparkle_timing", "sparkle_burst", "sparkle_colour",
                                                     "sparkle_colour_step", "palette", "sparkle_through",
                                                     "sparkle_time", "effect_brightness", "sparkle_brightness"};

static const int SLIDER_BASE = NUM_GLOBALS;          // slider1..slider8: the playing preset's
static const int NUM_NODES = SLIDER_BASE + MAXP;
static const int WORDS = (NUM_NODES + 31) / 32;

struct NodeInfo {
    char type;         // OSC type: 'i' 'f' 'T' (bool) 'N' (trigger) 'r' (colour)
    uint8_t access;    // 1 read, 2 write, 3 both
    bool range;
    float min, max;
    const char* help;
};

// The node's address and description; false when it doesn't exist. path may be null (the change
// check builds the address only for the few values it sends)
static bool nodeInfo(int id, NodeInfo& n, char* path, size_t len) {
    n = {'f', 3, false, 0, 1, ""};
    if (id < 0 || id >= NUM_NODES) return false;
    if (id < SLIDER_BASE) {
        if (path) snprintf(path, len, "/beamled/%s", GLOBAL_NAME[id]);
        switch (id) {
            case G_PRESET:
                n = {'i', 3, true, 1, (float)presetCount, "The preset playing, by its number on the web page. Fades over to it, and switches on."};
                break;
            case G_NEXT: n = {'N', 2, false, 0, 0, "The next preset, as a short press of the button (after the last comes the first). An argument of 0, a button's release, is ignored."}; break;
            case G_PREV: n = {'N', 2, false, 0, 0, "The previous preset. An argument of 0 is ignored."}; break;
            case G_ON: n = {'T', 3, false, 0, 1, "On or off, with a fade, as a long press of the button. No argument toggles."}; break;
            case G_BRIGHTNESS: n = {'f', 3, true, 0, 1, "Master brightness, 0..1, as the web page's Brightness slider."}; break;
            case G_AUTO: n = {'T', 3, false, 0, 1, "Auto: the presets change by themselves, every so many seconds or bars as set on the web page, among the presets that are in Auto. No argument toggles."}; break;
            case G_LINK: n = {'T', 3, false, 0, 1, "Ableton Link: Enabled (true) or Disabled (false). No argument toggles."}; break;
            case G_SPARKLE: n = {'f', 3, true, 0, 1, "Sparkle overlay, the same over every preset: twinkles on top of the effect (their colours: sparkle_colour). 0 = none, 1 = about 8% of the LEDs lit at a time. Not drawn over the Sparkle effect."}; break;
            case G_SPARKLE_TIMING: n = {'i', 3, true, 0, 6, "Sparkle overlay timing: 0 = free, twinkles start at any moment; otherwise they start only every 1 = 1 beat, 2 = 1/2 beat, 3 = 1/4 beat, 4 = 1/8 beat, 5 = 2 beats, 6 = 4 beats (a bar)."}; break;
            case G_SPARKLE_BURST: n = {'T', 3, false, 0, 1, "Sparkle overlay, with a timing other than free: true = a batch flashes on each tick and fades out before the next, false = single twinkles. No argument toggles."}; break;
            case G_SPARKLE_COLOUR: n = {'i', 3, true, 0, (float)(1 + paletteCount), "Sparkle overlay colours: 0 = white, 1 = the playing preset's palette, 2 = palette 1, 3 = palette 2, and so on (palette n = n + 1)."}; break;
            case G_SPARKLE_STEP: n = {'f', 3, true, 0, 1, "Sparkle overlay colour step: 0 = each twinkle a random colour from its palette; above 0 each new twinkle this far along the palette from the one before (0.5 = opposite colours in turn). Not with white twinkles."}; break;
            case G_PALETTE: n = {'i', 3, true, 0, (float)paletteCount, "Palette lock: 0 = every preset plays its own palette; 1, 2, ... = every preset plays that palette instead (their own settings are kept). Not saved: after a restart each preset has its own again."}; break;
            case G_SPARKLE_THROUGH: n = {'T', 3, false, 0, 1, "Sparkle overlay: true = each twinkle runs through its palette, first colour to last, over its life (a palette black, black, red: goes dark, pops red, fades); false = each twinkle one colour. Not with white twinkles. No argument toggles."}; break;
            case G_SPARKLE_TIME: n = {'f', 3, true, 0.2f, 5, "Sparkle overlay: how long a twinkle lasts, in seconds (with Through the palette, how long it takes to go through the palette). Not with bursts: they last until the next tick."}; break;
            case G_EFFECT_BRIGHTNESS: n = {'f', 3, true, 0, 1, "Effect brightness, 0..1: dims whatever preset plays (not the Sparkle overlay's twinkles), under the master brightness."}; break;
            case G_SPARKLE_BRIGHTNESS: n = {'f', 3, true, 0, 1, "Sparkle brightness, 0..1: how strongly the Sparkle overlay's twinkles show over the effect (0 = not at all, a dark twinkle darkens less), under the master brightness."}; break;
            case G_SAVE: n = {'N', 2, false, 0, 0, "Saves every preset as it plays now (as Save on each preset on the web page), with the palettes and settings, so they survive a power cut. Until then, preset changes from OSC or the web page play but are not saved."}; break;
        }
        return true;
    }
    if (path) snprintf(path, len, "/beamled/slider%d", id - SLIDER_BASE + 1);
    n = {'f', 3, true, 0, 1, "A slider of the preset playing, by its place in the list on the web page (slider1 = the first), 0..1 over its whole range. Ignored when the effect has fewer sliders. Changes play at once; Save keeps them."};
    return true;
}

// The playing preset's slider j: its effect's description, or null when it has none there
static const ParamDef* sliderDef(int j) {
    const EffectDef& ef = EFFECTS[presets[engineCurrent()].effect];
    return j >= 0 && j < ef.n ? &ef.params[j] : nullptr;
}

// The node's current value as 32 bits (float bits, int, 0/1, or 0xRRGGBBAA); false for
// triggers and nodes that don't exist
static bool nodeValue(int id, char type, uint32_t& v) {
    auto f = [&](float x) { memcpy(&v, &x, 4); return true; };
    auto i = [&](int x) { v = (uint32_t)x; return true; };
    if (type == 'N') return false;
    if (id < SLIDER_BASE) {
        switch (id) {
            case G_PRESET: return i(engineCurrent() + 1);
            case G_ON: return i(engineOn());
            case G_BRIGHTNESS: return f(settings.brightness / 255.f);
            case G_LINK: return i(linkSettings.enabled != 0);
            case G_AUTO: return i(autoSettings.enabled != 0);
            case G_SPARKLE: return f(sparkleSettings.amount);
            case G_SPARKLE_TIMING: return i(sparkleSettings.beat & SPARKLE_GRID);
            case G_SPARKLE_BURST: return i((sparkleSettings.beat & SPARKLE_BURST) != 0);
            case G_SPARKLE_COLOUR: return i(sparkleSettings.colour);
            case G_SPARKLE_STEP: return f(sparkleSettings.colourStep);
            case G_PALETTE: return i(paletteLock + 1);
            case G_SPARKLE_THROUGH: return i((sparkleSettings.beat & SPARKLE_THROUGH) != 0);
            case G_SPARKLE_TIME: return f(sparkleSettings.life);
            case G_EFFECT_BRIGHTNESS: return f(levels.effect / 255.f);
            case G_SPARKLE_BRIGHTNESS: return f(levels.sparkle / 255.f);
        }
        return false;
    }
    const int j = id - SLIDER_BASE;
    const ParamDef* d = sliderDef(j);
    if (!d) return f(0);
    return f(constrain((presets[engineCurrent()].p[j] - d->min) / (d->max - d->min), 0.f, 1.f));
}

// The node at an address, or -1
static int findNode(const char* path) {
    if (strncmp(path, "/beamled/", 9) != 0) return -1;
    const char* r = path + 9;
    for (int g = 0; g < NUM_GLOBALS; g++)
        if (!strcmp(r, GLOBAL_NAME[g])) return g;
    if (!strncmp(r, "slider", 6) && r[6] >= '1' && r[6] < '1' + MAXP && r[7] == 0) return SLIDER_BASE + (r[6] - '1');
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
        if (info.range && id >= SLIDER_BASE) v = constrain(v, info.min, info.max);
        if (info.type == 'i') v = roundf(v);
    }
    const bool on = v >= 0.5f;

    if (id < SLIDER_BASE) {
        const int cur = engineCurrent();
        switch (id) {
            case G_PRESET: engineSelect((int)v - 1); break;
            case G_NEXT: engineSelect((cur + 1) % presetCount); break;
            case G_PREV: engineSelect((cur + presetCount - 1) % presetCount); break;
            case G_ON: engineSetOn(on); break;
            case G_BRIGHTNESS: settings.brightness = (uint8_t)lroundf(constrain(v, 0.f, 1.f) * 255.f); break;
            case G_LINK: linkSettings.enabled = on; break;
            case G_SAVE: presetSaveAll(); storeMarkDirty(); break;
            case G_AUTO: autoSettings.enabled = on; storeMarkDirty(); webChanged(); break;
            case G_SPARKLE: sparkleSettings.amount = constrain(v, 0.f, 1.f); storeMarkDirty(); break;
            case G_SPARKLE_TIMING:
                sparkleSettings.beat = (sparkleSettings.beat & ~SPARKLE_GRID) | constrain((int)v, 0, 6);
                storeMarkDirty(); webChanged(); break;  // the page greys Bursts with free timing
            case G_SPARKLE_BURST:
                sparkleSettings.beat = (sparkleSettings.beat & ~SPARKLE_BURST) | (on ? SPARKLE_BURST : 0);
                storeMarkDirty(); break;
            case G_SPARKLE_COLOUR: sparkleSettings.colour = constrain((int)v, 0, 1 + paletteCount); storeMarkDirty(); webChanged(); break;
            case G_SPARKLE_STEP: sparkleSettings.colourStep = constrain(v, 0.f, 1.f); storeMarkDirty(); break;
            case G_PALETTE: paletteLock = constrain((int)lroundf(v), 0, paletteCount) - 1; webChanged(); break;  // not saved
            case G_SPARKLE_TIME: sparkleSettings.life = constrain(v, 0.2f, 5.f); storeMarkDirty(); break;
            case G_EFFECT_BRIGHTNESS: levels.effect = (uint8_t)lroundf(constrain(v, 0.f, 1.f) * 255.f); storeMarkDirty(); break;
            case G_SPARKLE_BRIGHTNESS: levels.sparkle = (uint8_t)lroundf(constrain(v, 0.f, 1.f) * 255.f); storeMarkDirty(); break;
            case G_SPARKLE_THROUGH:
                sparkleSettings.beat = (sparkleSettings.beat & ~SPARKLE_THROUGH) | (on ? SPARKLE_THROUGH : 0);
                storeMarkDirty(); webChanged(); break;  // the page greys Colour step with it
        }
        return;
    }
    // slider1..8: the playing preset's slider, 0..1 over its range (whole numbers stay whole)
    const int j = id - SLIDER_BASE;
    const ParamDef* d = sliderDef(j);
    if (!d) return;
    float x = d->min + v * (d->max - d->min);
    if (d->step >= 1) x = roundf(x);
    presets[engineCurrent()].p[j] = constrain(x, d->min, d->max);
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
static const size_t CHUNK = 1024;  // the tree is ~7.5 KB
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

// Container paths: / and /beamled
static bool isContainer(const char* path) { return !strcmp(path, "/") || !strcmp(path, "/beamled"); }

static void writeTree(Out& o, const char* path) {
    if (!strcmp(path, "/")) {
        static const char* const top[] = {"beamled"};
        writeContainer(o, "/", "beamLED", 0, 0, top, 1, "/%s");
    } else {
        writeContainer(o, path, "beamLED: the LED ceiling", 0, NUM_NODES);
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
                 "\"PATH_ADDED\":false,\"PATH_REMOVED\":false}}",
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
    bool closing = false;  // a send failed: closed, and skipped until onClose frees the slot
    bool page = false;  // the web page: also gets the status, the change counter and scan results
    bool preview = false;  // the page has its Preview open: gets the live frames
    char pageId[24] = "";  // which browser tab (it reconnects under the same id)
    uint32_t bits[WORDS];
};
static Listener listeners[MAX_LISTENERS];
static volatile int listenerCount = 0;
static volatile bool pushQueued = false;

static uint32_t* lastValue = nullptr;  // NUM_NODES, from the heap (static DRAM is full)
static uint32_t known[WORDS];  // lastValue holds what listeners were last sent

static inline bool hasBit(const uint32_t* b, int id) { return b[id >> 5] >> (id & 31) & 1; }
static inline void setBit(uint32_t* b, int id, bool on) {
    if (on) b[id >> 5] |= 1u << (id & 31);
    else b[id >> 5] &= ~(1u << (id & 31));
}

// A listener that can't take a frame (a phone asleep or gone) is closed at once: sending on would
// block the server's only task for the whole send timeout, every push, and nobody else gets an
// answer. The page reconnects by itself when it can.
static void sendFrame(int fd, httpd_ws_type_t type, const uint8_t* data, size_t len) {
    if (fd < 0) return;
    for (const Listener& l : listeners)
        if (l.fd == fd && l.closing) return;
    httpd_ws_frame_t f = {};
    f.final = true;
    f.type = type;
    f.payload = (uint8_t*)data;
    f.len = len;
    if (httpd_ws_send_frame_async(server, fd, &f) != ESP_OK) {
        Serial.printf("WebSocket %d not taking frames, closed\n", fd);
        for (Listener& l : listeners)
            if (l.fd == fd) l.closing = true;
        httpd_sess_trigger_close(server, fd);
    }
}

// A new subscription gets no value straight away: only changes from now on. MIMa's Learn takes
// the first message after its subscriptions as the one to learn, and the board can't answer 200
// subscriptions within the 0.5 s it ignores; clients that want the current value ask ?VALUE.
// The value as it is now counts as sent, so the next push doesn't send it either.
static void markCurrent(int id) {
    NodeInfo n;
    uint32_t v;
    if (hasBit(known, id) || !nodeInfo(id, n, nullptr, 0) || !nodeValue(id, n.type, v)) return;
    lastValue[id] = v;
    setBit(known, id, true);
}
static void sendStatus(int fd) {
    const String s = "{\"COMMAND\":\"STATUS\",\"DATA\":" + webStatusJson() + "}";
    sendFrame(fd, HTTPD_WS_TYPE_TEXT, (const uint8_t*)s.c_str(), s.length());
}

// Sends every change since last time to the listeners that follow it
// Changes go out as OSC bundles, one WebSocket frame per listener for everything that changed
// since the last push (split when a bundle fills a packet): each frame costs a network round
// trip on the board, each extra message in it almost nothing.
static const size_t BUNDLE_MAX = 1400;
static uint8_t* bundle = nullptr;  // one, from the heap: the listeners' bundles are built in turn
static size_t bundleLen = 0;

static void bundleFlush(int k) {
    if (bundleLen > 16 && k >= 0 && listeners[k].fd >= 0) sendFrame(listeners[k].fd, HTTPD_WS_TYPE_BINARY, bundle, bundleLen);
    // "#bundle", then the time tag 1 = at once
    static const uint8_t head[16] = {'#', 'b', 'u', 'n', 'd', 'l', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 1};
    memcpy(bundle, head, 16);
    bundleLen = 16;
}

static void bundleAdd(int k, const uint8_t* m, size_t len) {
    if (bundleLen + 4 + len > BUNDLE_MAX) bundleFlush(k);
    uint8_t* b = bundle + bundleLen;
    b[0] = len >> 24; b[1] = len >> 16; b[2] = len >> 8; b[3] = len;
    memcpy(b + 4, m, len);
    bundleLen += 4 + len;
}

static float pushAvgMs = 0;
static uint32_t pushMaxUs = 0;
void oscPushTiming(float& avgMs, float& maxMs) {
    avgMs = pushAvgMs;
    maxMs = pushMaxUs / 1000.f;
    pushMaxUs = 0;
}

// ---------------------------------------------------------------- live preview
// Two buffers on the heap, only while a page has its Preview open: the brightest of the frames
// rendered since the last preview frame (the preview sends 15 a second and the board renders 40, so
// a flash lit for a frame or two would mostly fall between two previews), and the frame to send.
// Both in 16-bit colour (RGB565), and at most pvMax points per row (the page asks: a small window
// can't show more): neighbouring LEDs merge into one point, the brightest of them.
// The render loop (core 1) and the server's task (core 0) hand them over by the state below, so a
// frame being sent is never written and buffers in use are never freed.
enum { PV_IDLE, PV_READY, PV_SENDING, PV_BUSY };
static std::atomic<int> pvState{PV_IDLE};
static uint16_t* pvPeak = nullptr;  // points per row * ny, RGB565
static uint8_t* pvOut = nullptr;    // "BLP5", points per row, ny (16 bits each), then the points
static size_t pvLen = 0;            // pvOut's length (0: no buffers)
static int pvGroup = 1;             // LEDs per point
static volatile int pvMax = 300;    // points per row at most, as the page asked
static bool pvHeld = false;
static uint32_t pvCalled = 0, pvLast = 0;

bool oscPreviewWanted() {
    for (const Listener& l : listeners)
        if (l.fd >= 0 && l.preview && !l.closing) return true;
    return false;
}

// Takes the buffers for the caller (IDLE or READY -> BUSY); false while a frame is going out
static bool pvTake(int& was) {
    for (int e : {PV_IDLE, PV_READY}) {
        int x = e;
        if (pvState.compare_exchange_strong(x, PV_BUSY)) { was = e; return true; }
    }
    return false;
}

static void pvFree() {
    free(pvPeak); free(pvOut);
    pvPeak = nullptr;
    pvOut = nullptr;
    pvLen = 0;
    pvHeld = false;
}

void oscPreviewFrame(const CRGB* leds, int nx, int ny, float bright) {
    int was;
    if (!pvTake(was)) return;  // the last frame is going out: this one waits for the next
    int next = was;
    const uint32_t now = millis();
    if (now - pvCalled > 200) pvHeld = false;  // after a pause the hold starts afresh
    pvCalled = now;
    const int max = constrain((int)pvMax, 16, 1024);
    const int g = (nx + max - 1) / max, ns = (nx + g - 1) / g;  // LEDs per point, points per row
    const size_t pts = (size_t)ns * ny, len = 8 + pts * 2;
    if (len != pvLen || g != pvGroup) {  // first use, another layout, or the page asked otherwise
        pvFree();
        pvPeak = (uint16_t*)malloc(pts * 2);
        pvOut = (uint8_t*)malloc(len);
        if (pvPeak && pvOut) pvLen = len, pvGroup = g;
        else pvFree();
        next = PV_IDLE;
    }
    if (pvLen) {
        const int kb = (int)(constrain(bright, 0.f, 1.f) * 256.f);
        if (!pvHeld) memset(pvPeak, 0, pts * 2);
        for (int b = 0; b < ny; b++) {
            const CRGB* row = leds + b * nx;
            uint16_t* out = pvPeak + b * ns;
            for (int i = 0; i < nx; i++) {
                const CRGB& c = row[i];
                const int r = (c.r * kb >> 8) >> 3, gr = (c.g * kb >> 8) >> 2, bl = (c.b * kb >> 8) >> 3;
                uint16_t& p = out[g == 1 ? i : i / g];
                const int pr = p >> 11, pg = p >> 5 & 63, pb = p & 31;  // the brightest, per colour
                p = (uint16_t)((r > pr ? r : pr) << 11 | (gr > pg ? gr : pg) << 5 | (bl > pb ? bl : pb));
            }
        }
        pvHeld = true;
        if (now - pvLast >= 66) {  // at most 15 a second
            memcpy(pvOut, "BLP5", 4);
            pvOut[4] = ns & 0xFF; pvOut[5] = ns >> 8; pvOut[6] = ny & 0xFF; pvOut[7] = ny >> 8;
            memcpy(pvOut + 8, pvPeak, pts * 2);  // little-endian, as the page reads it
            pvHeld = false;  // the next window starts afresh
            pvLast = now;
            next = PV_READY;
        }
    }
    pvState.store(next);
}

// Nobody watching (Preview folded, page gone): the buffers back to the heap
static void pvRelease() {
    int was;
    if (!pvLen || oscPreviewWanted() || !pvTake(was)) return;
    pvFree();
    pvState.store(PV_IDLE);
}

static void pushPreview() {
    int x = PV_READY;
    if (!pvState.compare_exchange_strong(x, PV_SENDING)) return;
    for (const Listener& l : listeners)
        if (l.fd >= 0 && l.preview && !l.closing) sendFrame(l.fd, HTTPD_WS_TYPE_BINARY, pvOut, pvLen);
    pvState.store(PV_IDLE);
}

static void pushChanges();
static void push(void*) {
    pushQueued = false;
    if (!listenerCount) return;
    const uint32_t t0 = micros();
    pushChanges();
    pushPreview();
    const uint32_t took = micros() - t0;
    if (took > pushMaxUs) pushMaxUs = took;
    pushAvgMs += (took / 1000.f - pushAvgMs) * 0.05f;
}

static void pushChanges() {
    // What changed since the last push
    uint32_t changed[WORDS] = {};
    NodeInfo n;
    for (int id = 0; id < NUM_NODES; id++) {
        uint32_t v;
        if (!nodeInfo(id, n, nullptr, 0) || !nodeValue(id, n.type, v)) continue;
        if (hasBit(known, id) && lastValue[id] == v) continue;
        lastValue[id] = v;
        setBit(known, id, true);
        setBit(changed, id, true);
    }
    // Each listener's share, as one bundle (or a few), in turn through the one buffer
    char p[64];
    for (int k = 0; k < MAX_LISTENERS; k++) {
        if (listeners[k].fd < 0) continue;
        bundleFlush(-1);  // fresh, empty
        for (int id = 0; id < NUM_NODES; id++) {
            if (!hasBit(changed, id) || !hasBit(listeners[k].bits, id) || !nodeInfo(id, n, p, sizeof(p))) continue;
            uint8_t m[96];
            const size_t len = encode(p, n.type, lastValue[id], m, sizeof(m));
            if (len) bundleAdd(k, m, len);
        }
        bundleFlush(k);
    }

    // The web page: the status once a second
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus < 1000) return;
    lastStatus = millis();
    for (Listener& l : listeners)
        if (l.fd >= 0 && l.page) sendStatus(l.fd);
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
            o.preview = false;
            memset(o.bits, 0, sizeof(o.bits));
            httpd_sess_trigger_close(server, o.fd);
        }
        l->page = true;
        strlcpy(l->pageId, id, sizeof(l->pageId));
        sendStatus(fd);
        return;
    }
    if (!strcmp(cmd, "PREVIEW")) {  // the page's Preview opened (true) or closed; MAX: points per row
        l->preview = l->page && (d["DATA"] | false);
        if (d["MAX"].is<int>()) pvMax = constrain(d["MAX"].as<int>(), 16, 1024);
        return;
    }
    const bool listen = !strcmp(cmd, "LISTEN");
    if (!listen && strcmp(cmd, "IGNORE")) return;
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
                slot->closing = false;
                slot->page = false;
                slot->preview = false;
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
        const int on = 1;  // Art-Net goes out from this socket too, and may be a broadcast
        setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    }

    chunkBuf = (char*)malloc(CHUNK);
    lastValue = (uint32_t*)calloc(NUM_NODES, sizeof(uint32_t));
    bundle = (uint8_t*)malloc(BUNDLE_MAX);
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
    c.send_wait_timeout = 2;  // s: a client that stops reading can't hold the server long
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
        if (!netServing()) return;  // the hotspot counts: with no cable it is the only way in
        start();
    }
    pvRelease();
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
