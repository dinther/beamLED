// beamLED: standalone LED ceiling renderer (WT32-ETH01 -> Art-Net -> PixLite).
// Effects work in real coordinates (metres) on the LEDs that exist, never on a dense grid.
#include <Arduino.h>

#include "artnet.h"
#include "config.h"
#include "effects.h"
#include "engine.h"
#include "link_clock.h"
#include "net.h"
#include "palette.h"
#include "store.h"
#include "web.h"

static CRGB leds[MAX_LEDS];
static CRGB lutWrap[256], lutLin[256];
static uint8_t outLut[256];
static float effState[MAXP];
static ArtNetOut artnet;

static bool isOn = true;
static float level = 0;      // master fade 0..1
static int pending = -1;     // preset to switch to once faded out
static float fps = 0, renderMs = 0;

static LinkClock* linkClock = nullptr;  // created once the network stack is up
static BeatTime beat;              // where the music is, this frame
static uint32_t linkAloneSince = 0;
static int linkPeers = 0;

static const float FADE_SWITCH_S = 0.4f;  // each way when changing preset
static const float FADE_ONOFF_S = 1.0f;
static const uint32_t LONG_PRESS_MS = 700;

// ---------------------------------------------------------------- engine

static void activate(int i) {
    settings.current = i;
    memset(effState, 0, sizeof(effState));
    storeMarkDirty();
    Serial.printf("Preset %d: %s\n", i + 1, presets[i].name);
}

void engineSelect(int i) {
    if (i < 0 || i >= NUM_PRESETS) return;
    isOn = true;
    pending = i != settings.current ? i : -1;
}

void engineSetOn(bool on) { isOn = on; }
bool engineOn() { return isOn; }
void engineResetEffect() { memset(effState, 0, sizeof(effState)); }
float engineFps() { return fps; }
float engineRenderMs() { return renderMs; }
int engineCurrent() { return pending >= 0 ? pending : settings.current; }
const BeatTime& engineBeat() { return beat; }

void engineSetTempo(double bpm) {
    linkSettings.bpm = bpm < 20 ? 20 : (bpm > 999 ? 999 : bpm);
    if (linkClock) linkClock->setTempo(linkSettings.bpm);
    storeMarkDirty();
}

static void printLayout() {
    Serial.printf("Layout %u x %u, %.2f m rows, aspect %.1f%s, row universes:", layout.nx, layout.ny,
                  layout.length, layout.aspect, layout.measured ? " (measured rows)" : "");
    for (int b = 0; b < layout.ny; b++) Serial.printf(" %u", layout.rowUni[b]);
    Serial.println();
}

void engineLayoutChanged() {
    layoutSanitize(layout);
    effectsInit();
    memset(effState, 0, sizeof(effState));
    memset(leds, 0, sizeof(leds));
    storeSaveLayout();
    printLayout();
}

static void updateFade(float dt) {
    if (pending >= 0) {
        level -= dt / FADE_SWITCH_S;
        if (level <= 0) {
            level = 0;
            activate(pending);
            pending = -1;
        }
        return;
    }
    const float target = isOn ? 1.f : 0.f;
    const float rate = dt / FADE_ONOFF_S;
    if (level < target) level = fminf(target, level + rate);
    else if (level > target) level = fmaxf(target, level - rate);
}

// Ableton Link, once a frame. As in Glow: Link sometimes stays alone while other apps see each
// other; only a fresh Link clears it, so while alone start afresh every 15 s.
static void updateLink() {
    if (!linkClock) return;
    linkClock->enable(linkSettings.enabled);
    beat = linkClock->now(linkSettings.latencyMs);
    if (beat.peers != linkPeers) {
        Serial.printf("Ableton Link: %d peer%s\n", beat.peers, beat.peers == 1 ? "" : "s");
        linkPeers = beat.peers;
    }
    const uint32_t now = millis();
    if (!beat.link || beat.peers > 0) {
        linkAloneSince = 0;
    } else if (!linkAloneSince) {
        linkAloneSince = now;
    } else if (now - linkAloneSince > 15000) {
        linkClock->rejoin();
        linkAloneSince = now;
    }
}

// Brightness accent on each beat: full on the beat, decaying until the next; the bar's first
// beat is strongest
static float beatPulse() {
    const float depth = presetSync[settings.current].pulse;
    if (depth <= 0) return 1.f;
    const double whole = floor(beat.beat);
    const float f = (float)(beat.beat - whole);
    double inBar = fmod(whole, beat.quantum);
    if (inBar < 0) inBar += beat.quantum;
    const float peak = inBar < 0.5 ? 1.f : 0.75f;
    return 1.f - depth + depth * peak * expf(-f * 5.f);
}

static void renderFrame(float dt) {
    Preset& p = presets[settings.current];
    const PresetSync& y = presetSync[settings.current];
    const Palette& pal = palettes[p.palette < NUM_PALETTES ? p.palette : 0];
    buildLut(pal, lutWrap, true);
    buildLut(pal, lutLin, false);
    const EffectDef& e = EFFECTS[p.effect < NUM_EFFECTS ? p.effect : 0];
    // Following the tempo, all motion runs at bpm/120 times the set speed
    const float edt = (y.tempo || y.lock) ? dt * (float)beat.bpm / 120.f : dt;
    Ctx c{p.p, edt, effState, lutWrap, lutLin, leds, beat.beat, (float)beat.bpm, y.lock != 0};
    e.render(c);
    if (!e.ownSparkle && p.sparkle > 0) sparkleOverlay(leds, p.sparkle, dt);
}

// Gamma, brightness and fade in one table. The fade is squared so it looks even to the eye.
static void buildOutLut() {
    const float B = settings.brightness / 255.f * level * level * beatPulse();
    for (int v = 0; v < 256; v++) outLut[v] = (uint8_t)(powf(v / 255.f, settings.gamma) * 255.f * B + 0.5f);
}

// ---------------------------------------------------------------- button

static void pollButton() {
    static bool down = false, longDone = false;
    static uint32_t changedAt = 0, downAt = 0;
    const bool now = digitalRead(cfg::BUTTON_PIN) == LOW;
    const uint32_t t = millis();
    if (now != down && t - changedAt > 30) {  // debounce
        changedAt = t;
        down = now;
        if (down) {
            downAt = t;
            longDone = false;
        } else if (!longDone) {  // short press
            if (!isOn) engineSetOn(true);
            else engineSelect(((pending >= 0 ? pending : settings.current) + 1) % NUM_PRESETS);
        }
    }
    if (down && !longDone && t - downAt >= LONG_PRESS_MS) {
        longDone = true;
        engineSetOn(!isOn);
    }
}

// ---------------------------------------------------------------- main

void setup() {
    Serial.begin(115200);
    Serial.println("\nbeamLED starting");
    pinMode(cfg::BUTTON_PIN, INPUT_PULLUP);
    storeLoad();
    netLoad();
    effectsInit();
    netBegin();
    linkClock = new LinkClock(linkSettings.bpm);
    webBegin();
    printLayout();
    Serial.printf("Preset %d, Art-Net to %s\n", settings.current + 1,
                  IPAddress(settings.target).toString().c_str());
}

void loop() {
    netLoop();
    webHandle();
    pollButton();
    storePoll();

    static uint32_t last = micros();
    const uint32_t now = micros();
    if (now - last < 1000000u / cfg::FPS) return;
    const float dt = fminf((now - last) / 1e6f, 0.1f);
    last = now;

    updateLink();
    updateFade(dt);
    const uint32_t t0 = micros();
    if (netOtaActive()) return;  // keep the network free for the update
    if (level > 0) renderFrame(dt);
    else memset(leds, 0, layoutLeds() * sizeof(CRGB));
    buildOutLut();
    const uint32_t t1 = micros();
    if (netCanSend()) artnet.send(IPAddress(settings.target), leds, outLut);

    // Stats, smoothed
    renderMs += ((t1 - t0) / 1000.f - renderMs) * 0.05f;
    fps += (1.f / fmaxf(dt, 0.001f) - fps) * 0.05f;
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 10000) {
        lastLog = millis();
        Serial.printf("%.1f fps, render %.1f ms, heap %u\n", fps, renderMs, ESP.getFreeHeap());
    }
}
