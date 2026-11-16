#include "web.h"

#include <ArduinoJson.h>
#include <WebServer.h>

#include "engine.h"
#include "link_clock.h"
#include "net.h"
#include "config.h"
#include "store.h"
#include "web_page.h"

static WebServer server(80);
static bool restartRequested = false;
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

static void sendJson(const JsonDocument& d) {
    String out;
    serializeJson(d, out);
    server.send(200, "application/json", out);
}

static bool readBody(JsonDocument& d) {
    if (deserializeJson(d, server.arg("plain"))) {
        server.send(400, "text/plain", "bad JSON");
        return false;
    }
    return true;
}

static void presetJson(JsonObject o, int i) {
    const Preset& p = presets[i];
    o["name"] = p.name;
    o["timing"] = presetSync[i].timing;
    o["beatsExp"] = presetSync[i].beatsExp;
    o["reverse"] = presetSync[i].reverse != 0;
    o["pulse"] = presetSync[i].pulse;
    o["effect"] = p.effect;
    o["palette"] = p.palette;
    o["sparkle"] = p.sparkle;
    JsonArray a = o["params"].to<JsonArray>();
    for (int j = 0; j < EFFECTS[p.effect].n; j++) a.add(p.p[j]);
}

static void paletteJson(JsonObject o, const Palette& p) {
    o["name"] = p.name;
    JsonArray a = o["colors"].to<JsonArray>();
    for (int j = 0; j < p.n; j++) a.add(hexColour(p.c[j]));
}

static void statusJson(JsonObject o) {
    o["on"] = engineOn();
    o["current"] = engineCurrent();
    o["fps"] = roundf(engineFps() * 10) / 10;
    o["renderMs"] = roundf(engineRenderMs() * 10) / 10;
    o["net"] = netModeText();
    o["wifi"] = netWifiText();
    o["heap"] = ESP.getFreeHeap();
    const BeatTime& b = engineBeat();
    JsonObject l = o["link"].to<JsonObject>();
    l["enabled"] = linkSettings.enabled != 0;
    l["bpm"] = round(b.bpm * 10) / 10;
    l["peers"] = b.peers;
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
    JsonArray ef = d["effects"].to<JsonArray>();
    for (int i = 0; i < NUM_EFFECTS; i++) {
        JsonObject o = ef.add<JsonObject>();
        o["name"] = EFFECTS[i].name;
        o["sparkle"] = !EFFECTS[i].ownSparkle;
        if (EFFECTS[i].cycle) o["cycle"] = EFFECTS[i].cycle;
        o["speedParam"] = EFFECTS[i].speedParam;
        JsonArray ps = o["params"].to<JsonArray>();
        for (int j = 0; j < EFFECTS[i].n; j++) {
            const ParamDef& pd = EFFECTS[i].params[j];
            JsonObject q = ps.add<JsonObject>();
            q["name"] = pd.name;
            q["min"] = pd.min;
            q["max"] = pd.max;
            q["step"] = pd.step;
            q["def"] = pd.def;
        }
    }
    JsonArray pl = d["palettes"].to<JsonArray>();
    for (const Palette& p : palettes) paletteJson(pl.add<JsonObject>(), p);
    JsonArray pr = d["presets"].to<JsonArray>();
    for (int i = 0; i < NUM_PRESETS; i++) presetJson(pr.add<JsonObject>(), i);
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
    if (d["gamma"].is<float>()) settings.gamma = constrain(d["gamma"].as<float>(), 1.f, 3.f);
    if (d["target"].is<const char*>()) {
        IPAddress ip;
        if (ip.fromString(d["target"].as<const char*>())) settings.target = (uint32_t)ip;
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
    if (i < 0 || i >= NUM_PRESETS) { server.send(400, "text/plain", "bad index"); return; }
    Preset& p = presets[i];
    if (d["name"].is<const char*>()) strlcpy(p.name, d["name"].as<const char*>(), sizeof(p.name));
    if (d["effect"].is<int>()) {
        const int e = d["effect"].as<int>();
        if (e >= 0 && e < NUM_EFFECTS && e != p.effect) {
            presetSetEffect(p, e);
            if (i == settings.current) engineResetEffect();
        }
    }
    if (d["palette"].is<int>()) p.palette = constrain(d["palette"].as<int>(), 0, NUM_PALETTES - 1);
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
    storeMarkDirty();
    JsonDocument r;
    presetJson(r.to<JsonObject>(), i);
    sendJson(r);
}

static void handlePalette() {
    JsonDocument d;
    if (!readBody(d)) return;
    const int i = d["index"] | -1;
    if (i < 0 || i >= NUM_PALETTES) { server.send(400, "text/plain", "bad index"); return; }
    Palette& p = palettes[i];
    if (d["name"].is<const char*>()) strlcpy(p.name, d["name"].as<const char*>(), sizeof(p.name));
    if (d["colors"].is<JsonArray>()) {
        JsonArray a = d["colors"].as<JsonArray>();
        if (a.size() >= 2) {
            p.n = min((int)a.size(), PAL_MAX);
            for (int j = 0; j < p.n; j++) p.c[j] = parseColour(a[j].as<const char*>());
        }
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
        server.send(400, "text/plain", "at most " + String(MAX_LEDS) + " LEDs");
        return;
    }
    layout = l;
    engineLayoutChanged();
    handleState();
}

// Ableton Link: enabled, tempo (with Link enabled it changes everyone's), output latency
static void handleLink() {
    JsonDocument d;
    if (!readBody(d)) return;
    if (d["enabled"].is<bool>()) linkSettings.enabled = d["enabled"].as<bool>();
    if (d["bpm"].is<float>()) engineSetTempo(d["bpm"].as<float>());
    if (d["latency"].is<float>()) linkSettings.latencyMs = constrain(d["latency"].as<float>(), 0.f, 500.f);
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
        else { server.send(400, "text/plain", "bad fallback IP"); return; }
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
        server.send(400, "text/plain", "hotspot password needs 8 or more characters");
        return;
    }
    netSave();
    server.send(200, "application/json", "{\"restarting\":true}");
    restartRequested = true;
    restartAt = millis();
}

static void handleScan() {
    if (server.method() == HTTP_POST) netRequestScan();
    String out = "{\"state\":" + String(netScanState()) + ",\"nets\":" + netScanResults() + "}";
    server.send(200, "application/json", out);
}

void webBegin() {
    server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", WEB_PAGE); });
    server.on("/api/state", HTTP_GET, handleState);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/set", HTTP_POST, handleSet);
    server.on("/api/preset", HTTP_POST, handlePreset);
    server.on("/api/palette", HTTP_POST, handlePalette);
    server.on("/api/layout", HTTP_POST, handleLayout);
    server.on("/api/link", HTTP_POST, handleLink);
    server.on("/api/net", HTTP_GET, handleNetGet);
    server.on("/api/net", HTTP_POST, handleNetSet);
    server.on("/api/scan", handleScan);
    // Hotspot: phones probe random URLs to detect a captive portal; send them to the page
    server.onNotFound([]() {
        server.sendHeader("Location", "http://" + netCurrentIp().toString() + "/");
        server.send(302, "text/plain", "");
    });
    server.begin();
}

void webHandle() {
    server.handleClient();
    if (restartRequested && millis() - restartAt > 500) ESP.restart();
}
