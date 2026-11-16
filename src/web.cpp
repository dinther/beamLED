#include "web.h"

#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <lwip/sockets.h>

#include "engine.h"
#include "link_clock.h"
#include "net.h"
#include "osc.h"
#include "config.h"
#include "store.h"
#include "web_page.h"

static bool restartRequested = false;
// Counts changes the page shows but OSC doesn't carry (names, the preset list, layout, ...):
// open pages reload when it moves. Values (brightness, sliders, colours) reach them as OSC.
static volatile uint32_t revision = 0;
void webChanged() { revision = revision + 1; }
static uint32_t restartAt = 0;

static String hexColour(const CRGB& c) {
    char b[8];
    snprintf(b, sizeof(b), "#%02x%02x%02x", c.r, c.g, c.b);
    return b;
}

static CRGB parseColour(const char* s) {
    if (!s || s[0] != '#') return CRGB::Black;
    return CRGB((uint32_t)strtoul(s + 1, nullptr, 16));
}

// The web page and its API are served by the ESP-IDF HTTP server (osc.cpp runs it, on port 80,
// with OSCQuery), on its own task on core 0: several connections at once, so a browser's idle
// spare connection never holds up a request, and nothing here ever holds up a frame. Each API
// handler runs under the engine lock and only records its reply, which goes out after the lock
// is released.
static httpd_req_t* rq = nullptr;  // the request being handled
static String rqBody, rqQuery;
static int replyCode = 0;
static const char* replyType = "text/plain";
static String replyBody;
static bool replyCache = false;  // the same for this firmware build: may be cached for good
// A JSON reply is kept as the document and written out in 1 KB pieces as it is serialised: a
// 16 KB String needs one free block that big, and when the heap is in pieces it stops growing
// and the reply goes out cut off.
static JsonDocument replyDoc;
static bool replyJson = false;

static void reply(int code, const char* type, const String& body) {
    replyCode = code;
    replyType = type;
    replyBody = body;
    replyJson = false;
}

static bool hasArg(const char* key) {
    char v[48];
    return httpd_query_key_value(rqQuery.c_str(), key, v, sizeof(v)) == ESP_OK;
}

static String arg(const char* key) {
    char v[48];
    return httpd_query_key_value(rqQuery.c_str(), key, v, sizeof(v)) == ESP_OK ? String(v) : String();
}

static void sendJson(JsonDocument& d) {
    replyCode = 200;
    replyType = "application/json";
    replyBody = String();
    replyDoc = std::move(d);
    replyJson = true;
}

// Serialised JSON straight into the HTTP reply, 1 KB at a time
struct ChunkOut {
    httpd_req_t* req;
    char buf[1024];
    size_t n = 0;
    esp_err_t err = ESP_OK;
    void flush() {
        if (n && err == ESP_OK) err = httpd_resp_send_chunk(req, buf, n);
        n = 0;
    }
    size_t write(uint8_t c) {
        buf[n++] = (char)c;
        if (n == sizeof(buf)) flush();
        return 1;
    }
    size_t write(const uint8_t* s, size_t len) {
        for (size_t i = 0; i < len; i++) write(s[i]);
        return len;
    }
};

static bool readBody(JsonDocument& d) {
    if (deserializeJson(d, rqBody)) {
        reply(400, "text/plain", "bad JSON");
        return false;
    }
    return true;
}

static void presetJson(JsonObject o, int i) {
    const Preset& p = presets[i];
    o["name"] = p.name;
    o["changed"] = presetChanged(i);  // unsaved edits
    if (presetFresh(i)) o["fresh"] = true;  // just added: Save keeps a new name in place
    if (strcmp(p.name, presetSavedName(i)) != 0 && !presetFresh(i))
        o["savedName"] = presetSavedName(i);  // renamed: Save makes a new preset
    o["timing"] = presetSync[i].timing;
    o["beatsExp"] = presetSync[i].beatsExp;
    o["reverse"] = presetSync[i].reverse != 0;
    o["pulse"] = presetSync[i].pulse;
    o["effect"] = p.effect;
    o["palette"] = p.palette;
    o["sparkle"] = p.sparkle;
    JsonArray a = o["params"].to<JsonArray>();
    for (int j = 0; j < EFFECTS[p.effect].n; j++) a.add(p.p[j]);
    JsonArray ms = o["motions"].to<JsonArray>();
    for (const Motion& mo : presetMotion[i]) {
        JsonObject m = ms.add<JsonObject>();
        m["target"] = mo.target;
        m["shape"] = mo.shape;
        m["inBeats"] = mo.inBeats != 0;
        m["beatsExp"] = mo.beatsExp;
        m["from"] = mo.from;
        m["to"] = mo.to;
        m["seconds"] = mo.seconds;
        m["phase"] = mo.phase;
    }
}

static void paletteJson(JsonObject o, const Palette& p) {
    o["name"] = p.name;
    JsonArray a = o["colors"].to<JsonArray>();
    for (int j = 0; j < p.n; j++) a.add(hexColour(p.c[j]));
    JsonArray s = o["stops"].to<JsonArray>();  // where each colour sits, 0..1
    for (int j = 0; j < p.n; j++) s.add(roundf(p.pos[j] * 1000) / 1000);
}

static void statusJson(JsonObject o) {
    o["on"] = engineOn();
    o["current"] = engineCurrent();
    o["presetCount"] = presetCount;
    o["brightness"] = settings.brightness;
    o["rev"] = revision;
    uint32_t changed = 0;  // presets with unsaved edits, one bit each
    for (int i = 0; i < presetCount; i++)
        if (presetChanged(i)) changed |= 1u << i;
    o["changed"] = changed;
    // The playing preset's motions: phase now and cycles per second, so the page can animate them
    JsonArray mo = o["motion"].to<JsonArray>();
    for (int m = 0; m < MOTIONS; m++) {
        float ph, rate;
        engineMotion(m, ph, rate);
        JsonArray a = mo.add<JsonArray>();
        a.add(ph);
        a.add(rate);
    }
    o["fps"] = roundf(engineFps() * 10) / 10;
    o["renderMs"] = roundf(engineRenderMs() * 10) / 10;
    o["sendMs"] = roundf(engineSendMs() * 10) / 10;
    o["net"] = netModeText();
    o["wifi"] = netWifiText();
    o["heap"] = ESP.getFreeHeap();
    const BeatTime& b = engineBeat();
    JsonObject l = o["link"].to<JsonObject>();
    l["enabled"] = linkSettings.enabled != 0;
    l["bpm"] = round(b.bpm * 10) / 10;
    l["peers"] = b.peers;
    o["osc"] = oscStatusText();
}

String webStatusJson() {
    JsonDocument d;
    statusJson(d.to<JsonObject>());
    String s;
    serializeJson(d, s);
    return s;
}

static const char BUILD[] = __DATE__ " " __TIME__;

static void handleEffects() {
    JsonDocument d;
    JsonArray ef = d.to<JsonArray>();
    for (int i = 0; i < NUM_EFFECTS; i++) {
        JsonObject o = ef.add<JsonObject>();
        o["name"] = EFFECTS[i].name;
        o["sparkle"] = !EFFECTS[i].ownSparkle;
        if (EFFECTS[i].cycle) o["cycle"] = EFFECTS[i].cycle;
        o["speedParam"] = EFFECTS[i].speedParam;
        o["help"] = EFFECTS[i].help;
        JsonArray ps = o["params"].to<JsonArray>();
        for (int j = 0; j < EFFECTS[i].n; j++) {
            const ParamDef& pd = EFFECTS[i].params[j];
            JsonObject q = ps.add<JsonObject>();
            q["name"] = pd.name;
            q["help"] = pd.help;
            q["min"] = pd.min;
            q["max"] = pd.max;
            q["step"] = pd.step;
            q["def"] = pd.def;
        }
    }
    sendJson(d);
    replyCache = true;
}

static void handleState() {
    JsonDocument d;
    statusJson(d.to<JsonObject>());
    d["brightness"] = settings.brightness;
    d["gamma"] = settings.gamma;
    d["target"] = IPAddress(settings.target).toString();
    JsonObject lay = d["layout"].to<JsonObject>();
    lay["nx"] = layout.nx;
    lay["ny"] = layout.ny;
    lay["length"] = layout.length;
    lay["aspect"] = layout.aspect;
    lay["measured"] = layout.measured != 0;
    JsonArray ru = lay["rowUni"].to<JsonArray>();
    for (int b = 0; b < layout.ny; b++) ru.add(layout.rowUni[b]);
    lay["maxLeds"] = MAX_LEDS;
    lay["measuredRows"] = cfg::CEILING_ROWS;
    d["build"] = BUILD;  // the page gets the effects from /api/effects?v=<build>, cached
    JsonArray pl = d["palettes"].to<JsonArray>();
    for (int k = 0; k < paletteCount; k++) paletteJson(pl.add<JsonObject>(), palettes[k]);
    JsonArray pr = d["presets"].to<JsonArray>();
    for (int i = 0; i < presetCount; i++) presetJson(pr.add<JsonObject>(), i);
    d["maxPresets"] = MAX_PRESETS;
    d["maxPalettes"] = MAX_PALETTES;
    d["linkLatency"] = linkSettings.latencyMs;
    sendJson(d);
}

static void handleStatus() {
    JsonDocument d;
    statusJson(d.to<JsonObject>());
    sendJson(d);
}

static void handleSet() {
    JsonDocument d;
    if (!readBody(d)) return;
    if (d["brightness"].is<int>()) settings.brightness = constrain(d["brightness"].as<int>(), 0, 255);
    if (d["gamma"].is<float>()) {
        settings.gamma = constrain(d["gamma"].as<float>(), 1.f, 3.f);
        webChanged();
    }
    if (d["target"].is<const char*>()) {
        IPAddress ip;
        if (ip.fromString(d["target"].as<const char*>())) settings.target = (uint32_t)ip;
        webChanged();
    }
    if (d["current"].is<int>()) engineSelect(d["current"].as<int>());
    if (d["on"].is<bool>()) engineSetOn(d["on"].as<bool>());
    storeMarkDirty();
    handleStatus();
}

static void handlePreset() {
    JsonDocument d;
    if (!readBody(d)) return;
    const int i = d["index"] | -1;
    if (i < 0 || i >= presetCount) { reply(400, "text/plain", "bad index"); return; }
    Preset& p = presets[i];
    // {"motion": m, ...fields}: one motion's settings, and nothing else. Its fields share names
    // with the preset's own (beatsExp: a motion's period in beats, or the preset's beats per cycle),
    // so they are never read as the preset's.
    if (d["motion"].is<int>()) {
        const int m = d["motion"].as<int>();
        if (m < 0 || m >= MOTIONS) { reply(400, "text/plain", "bad motion"); return; }
        Motion& mo = presetMotion[i][m];
        const EffectDef& e = EFFECTS[p.effect];
        if (d["target"].is<int>()) mo.target = constrain(d["target"].as<int>(), 0, (int)e.n);
        if (d["shape"].is<int>()) mo.shape = constrain(d["shape"].as<int>(), 0, NUM_SHAPES - 1);
        if (d["inBeats"].is<bool>()) mo.inBeats = d["inBeats"].as<bool>();
        if (d["beatsExp"].is<int>()) mo.beatsExp = constrain(d["beatsExp"].as<int>(), 0, 6);
        if (d["seconds"].is<float>()) mo.seconds = constrain(d["seconds"].as<float>(), 0.25f, 600.f);
        if (d["phase"].is<float>()) mo.phase = constrain(d["phase"].as<float>(), 0.f, 1.f);
        if (d["from"].is<float>()) mo.from = d["from"].as<float>();
        if (d["to"].is<float>()) mo.to = d["to"].as<float>();
        if (mo.target) {  // the ends within the slider's range
            const ParamDef& pd = e.params[mo.target - 1];
            mo.from = constrain(mo.from, pd.min, pd.max);
            mo.to = constrain(mo.to, pd.min, pd.max);
        }
        JsonDocument r;
        presetJson(r.to<JsonObject>(), i);
        sendJson(r);
        return;
    }
    if (d["name"].is<const char*>()) {
        presetRename(i, d["name"].as<const char*>());  // an edit: Save makes it a new preset
        webChanged();
    }
    // Save: what plays becomes the preset. Revert: back to the saved version.
    int savedAs = -1;  // Save with a new name: the new preset's index
    if (d["save"] | false) {
        const int k = presetSave(i);
        if (k >= 0 && k != i) {
            savedAs = k;
            webChanged();
            if (i == settings.current) engineResetEffect();  // it went back to what was saved
            engineSelect(k);                                  // the edits play on, as the new preset
        }
    }
    if (d["revert"] | false) {
        const uint8_t was = p.effect;
        presetRevert(i);
        webChanged();
        if (i == settings.current && p.effect != was) engineResetEffect();
    }
    if (d["effect"].is<int>()) {
        const int e = d["effect"].as<int>();
        if (e >= 0 && e < NUM_EFFECTS && e != p.effect) {
            presetChangeEffect(i, e);  // its motions go off: their sliders are gone
            webChanged();
            if (i == settings.current) engineResetEffect();
        }
    }
    if (d["palette"].is<int>()) p.palette = constrain(d["palette"].as<int>(), 0, paletteCount - 1);
    if (d["sparkle"].is<float>()) p.sparkle = constrain(d["sparkle"].as<float>(), 0.f, 1.f);
    if (d["params"].is<JsonArray>()) {
        JsonArray a = d["params"].as<JsonArray>();
        const EffectDef& e = EFFECTS[p.effect];
        for (int j = 0; j < e.n && j < (int)a.size(); j++)
            p.p[j] = constrain(a[j].as<float>(), e.params[j].min, e.params[j].max);
    }
    if (d["timing"].is<int>()) presetSync[i].timing = constrain(d["timing"].as<int>(), 0, (int)TIMING_BEATS);
    if (d["beatsExp"].is<int>()) presetSync[i].beatsExp = constrain(d["beatsExp"].as<int>(), 0, 6);
    if (d["reverse"].is<bool>()) presetSync[i].reverse = d["reverse"].as<bool>();
    if (d["pulse"].is<float>()) presetSync[i].pulse = constrain(d["pulse"].as<float>(), 0.f, 1.f);
    // Edits play at once and stay unsaved until Save: nothing to write to flash here
    JsonDocument r;
    presetJson(r.to<JsonObject>(), savedAs >= 0 ? savedAs : i);
    if (savedAs >= 0) r["savedAs"] = savedAs;
    sendJson(r);
}

// The preset list: {"add": from} copies preset `from` to the end (and plays it),
// {"delete": i}, {"move": i, "to": j}. Answers with the whole state.
static void handlePresetList() {
    JsonDocument d;
    if (!readBody(d)) return;
    bool ok = false;
    if (d["add"].is<int>()) {
        const int i = presetAdd(d["add"].as<int>());
        ok = i >= 0;
        if (ok) engineSelect(i);
    } else if (d["delete"].is<int>()) {
        ok = engineDeletePreset(d["delete"].as<int>());
    } else if (d["move"].is<int>() && d["to"].is<int>()) {
        ok = engineMovePreset(d["move"].as<int>(), d["to"].as<int>());
    }
    if (!ok) { reply(400, "text/plain", "not possible"); return; }
    webChanged();
    handleState();
}

// The palette list: {"add": from} copies palette rom to the end, {"delete": k}. Answers with the
// whole state.
static void handlePaletteList() {
    JsonDocument d;
    if (!readBody(d)) return;
    bool ok = false;
    if (d["add"].is<int>()) ok = paletteAdd(d["add"].as<int>()) >= 0;
    else if (d["delete"].is<int>()) ok = paletteDelete(d["delete"].as<int>());
    if (!ok) { reply(400, "text/plain", "not possible"); return; }
    webChanged();
    handleState();
}

static void handlePalette() {
    JsonDocument d;
    if (!readBody(d)) return;
    const int i = d["index"] | -1;
    if (i < 0 || i >= paletteCount) { reply(400, "text/plain", "bad index"); return; }
    Palette& p = palettes[i];
    if (d["name"].is<const char*>()) {
        strlcpy(p.name, d["name"].as<const char*>(), sizeof(p.name));
        webChanged();
    }
    if (d["colors"].is<JsonArray>()) {
        JsonArray a = d["colors"].as<JsonArray>();
        if (a.size() >= 2) {
            if (p.n != min((int)a.size(), PAL_MAX)) webChanged();
            p.n = min((int)a.size(), PAL_MAX);
            for (int j = 0; j < p.n; j++) p.c[j] = parseColour(a[j].as<const char*>());
            paletteEvenStops(p);  // unless stops come with them
        }
    }
    // stops: where each colour sits along the palette, 0..1 (kept in order)
    if (d["stops"].is<JsonArray>()) {
        JsonArray s = d["stops"].as<JsonArray>();
        for (int j = 0; j < p.n && j < (int)s.size(); j++) p.pos[j] = s[j].as<float>();
        paletteSanitizeStops(p);
    }
    storeMarkDirty();
    JsonDocument r;
    paletteJson(r.to<JsonObject>(), p);
    sendJson(r);
}

// Takes effect at once; presets and palettes are kept
static void handleLayout() {
    JsonDocument d;
    if (!readBody(d)) return;
    Layout l = layout;
    if (d["nx"].is<int>()) l.nx = constrain(d["nx"].as<int>(), 1, MAX_X);
    if (d["ny"].is<int>()) l.ny = constrain(d["ny"].as<int>(), 1, MAX_Y);
    if (d["length"].is<float>()) l.length = d["length"].as<float>();
    if (d["aspect"].is<float>()) l.aspect = d["aspect"].as<float>();
    if (d["measured"].is<bool>()) l.measured = d["measured"].as<bool>();
    // rowUni: start universe per row; rows not listed continue after the last one listed
    if (d["rowUni"].is<JsonArray>()) {
        JsonArray a = d["rowUni"].as<JsonArray>();
        const int n = min((int)a.size(), MAX_Y);
        for (int b = 0; b < n; b++) l.rowUni[b] = constrain(a[b].as<int>(), 0, 32767);
        layoutFillUniverses(l, n, n ? l.rowUni[0] : 0);
    }
    if (l.nx * l.ny > MAX_LEDS) {
        reply(400, "text/plain", "at most " + String(MAX_LEDS) + " LEDs");
        return;
    }
    layout = l;
    engineLayoutChanged();
    webChanged();
    handleState();
}

// Ableton Link: enabled, tempo (with Link enabled it changes everyone's), output latency
static void handleLink() {
    JsonDocument d;
    if (!readBody(d)) return;
    if (d["enabled"].is<bool>()) linkSettings.enabled = d["enabled"].as<bool>();
    if (d["bpm"].is<float>()) engineSetTempo(d["bpm"].as<float>());
    if (d["latency"].is<float>()) {
        linkSettings.latencyMs = constrain(d["latency"].as<float>(), 0.f, 500.f);
        webChanged();
    }
    storeMarkDirty();
    handleStatus();
}

static void handleNetGet() {
    JsonDocument d;
    d["name"] = netSettings.name;
    d["fallback"] = (uint32_t)netSettings.fallbackIp ? netSettings.fallbackIp.toString() : String("");
    d["autoFallback"] = netAutoFallbackIp().toString();
    d["ssid"] = netSettings.wifiSsid;
    d["wifiPassSet"] = netSettings.wifiPass.length() > 0;
    d["apPassSet"] = netSettings.apPass.length() > 0;
    sendJson(d);
}

// Network settings take effect after a restart. Empty password fields keep the saved password;
// "-" clears it.
static void handleNetSet() {
    JsonDocument d;
    if (!readBody(d)) return;
    if (d["name"].is<const char*>()) netSettings.name = d["name"].as<const char*>();
    if (d["fallback"].is<const char*>()) {
        IPAddress ip;
        String s = d["fallback"].as<const char*>();
        s.trim();
        if (!s.length()) netSettings.fallbackIp = IPAddress(0, 0, 0, 0);
        else if (ip.fromString(s)) netSettings.fallbackIp = ip;
        else { reply(400, "text/plain", "bad fallback IP"); return; }
    }
    if (d["ssid"].is<const char*>()) {
        netSettings.wifiSsid = d["ssid"].as<const char*>();
        if (!netSettings.wifiSsid.length()) netSettings.wifiPass = "";
    }
    auto pass = [](JsonVariant v, String& into) {
        if (!v.is<const char*>()) return;
        String s = v.as<const char*>();
        if (s == "-") into = "";
        else if (s.length()) into = s;
    };
    pass(d["wifiPass"], netSettings.wifiPass);
    pass(d["apPass"], netSettings.apPass);
    if (netSettings.apPass.length() && netSettings.apPass.length() < 8) {
        reply(400, "text/plain", "hotspot password needs 8 or more characters");
        return;
    }
    netSave();
    reply(200, "application/json", "{\"restarting\":true}");
    restartRequested = true;
    restartAt = millis();
}

static void handleScan() {
    if (rq->method == HTTP_POST) netRequestScan();
    String out = "{\"state\":" + String(netScanState()) + ",\"nets\":" + netScanResults() + "}";
    reply(200, "application/json", out);
}

// /api/bench?e=<effect>&frames=<n>: ms per frame to render it (palette 1, default parameters).
// Holds the render loop for that long, so ask for one effect at a time.
static void handleBench() {
    static CRGB lw[256], ll[256];
    buildLut(palettes[0], lw, true);
    buildLut(palettes[0], ll, false);
    const int e = hasArg("e") ? arg("e").toInt() : 0;
    const int frames = constrain(hasArg("frames") ? arg("frames").toInt() : 5, 1, 40);
    JsonDocument d;
    // How many network connections (lwIP sockets, 16 in all) are free right now
    int fds[16], n = 0;
    while (n < 16 && (fds[n] = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) n++;
    for (int k = 0; k < n; k++) close(fds[k]);
    d["socketsFree"] = n;
    d["effect"] = e < NUM_EFFECTS && e >= 0 ? EFFECTS[e].name : "?";
    d["ms"] = effectsBenchmark(e, lw, ll, frames);
    sendJson(d);
}

struct Route {
    const char* path;
    int method;  // HTTP_GET, HTTP_POST, or -1 for both
    void (*handler)();
};
static const Route ROUTES[] = {
    {"/api/state", HTTP_GET, handleState},       {"/api/effects", HTTP_GET, handleEffects},
    {"/api/status", HTTP_GET, handleStatus},     {"/api/set", HTTP_POST, handleSet},
    {"/api/preset", HTTP_POST, handlePreset},    {"/api/presets", HTTP_POST, handlePresetList},
    {"/api/palette", HTTP_POST, handlePalette},  {"/api/palettes", HTTP_POST, handlePaletteList},
     {"/api/layout", HTTP_POST, handleLayout},
    {"/api/link", HTTP_POST, handleLink},        {"/api/net", HTTP_GET, handleNetGet},
    {"/api/net", HTTP_POST, handleNetSet},       {"/api/scan", -1, handleScan},
    {"/api/bench", HTTP_GET, handleBench},
};

static const char* statusLine(int code) {
    switch (code) {
        case 200: return "200 OK";
        case 302: return "302 Found";
        case 400: return "400 Bad Request";
        case 404: return "404 Not Found";
        default: return "500 Internal Server Error";
    }
}

// The page itself, for a browser asking for /
esp_err_t webPage(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, WEB_PAGE, strlen_P(WEB_PAGE));
}

// Anything a browser asks for that isn't there: the page (phones probe random addresses on the
// hotspot to find a captive portal)
esp_err_t webRedirect(httpd_req_t* req) {
    const String to = "http://" + netCurrentIp().toString() + "/";
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", to.c_str());
    return httpd_resp_send(req, nullptr, 0);
}

// /api/...: false when there is no such address (the caller answers)
bool webApi(httpd_req_t* req, esp_err_t& result) {
    char path[48];
    strlcpy(path, req->uri, sizeof(path));
    if (char* q = strchr(path, '?')) *q = 0;
    const Route* route = nullptr;
    for (const Route& r : ROUTES)
        if (!strcmp(path, r.path) && (r.method < 0 || r.method == (int)req->method)) route = &r;
    if (!route) return false;

    rqQuery = "";
    const size_t ql = httpd_req_get_url_query_len(req);
    if (ql) {
        char* q = (char*)malloc(ql + 1);
        if (q && httpd_req_get_url_query_str(req, q, ql + 1) == ESP_OK) rqQuery = q;
        free(q);
    }
    rqBody = "";
    if (req->content_len > 0 && req->content_len <= 8192) {
        char* b = (char*)malloc(req->content_len + 1);
        size_t got = 0;
        while (b && got < req->content_len) {
            const int n = httpd_req_recv(req, b + got, req->content_len - got);
            if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (n <= 0) break;
            got += n;
        }
        if (b) {
            b[got] = 0;
            rqBody = b;
        }
        free(b);
    }

    rq = req;
    replyCode = 0;
    replyCache = false;
    replyJson = false;
    {
        EngineLock lock;
        route->handler();
    }
    rq = nullptr;
    rqBody = String();
    if (!replyCode) reply(500, "text/plain", "no reply");
    httpd_resp_set_status(req, statusLine(replyCode));
    httpd_resp_set_type(req, replyType);
    if (replyCache) httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");
    if (replyJson) {
        ChunkOut out{req};
        serializeJson(replyDoc, out);
        out.flush();
        result = out.err == ESP_OK ? httpd_resp_send_chunk(req, nullptr, 0) : out.err;
        replyDoc.clear();
        replyJson = false;
    } else {
        result = httpd_resp_send(req, replyBody.c_str(), replyBody.length());
    }
    replyBody = String();
    return true;
}

void webBegin() {}  // the HTTP server is started with OSCQuery, once the network is up

void webHandle() {
    if (restartRequested && millis() - restartAt > 500) ESP.restart();
}
