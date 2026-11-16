// beamLED: standalone LED ceiling renderer (WT32-ETH01 -> Art-Net -> PixLite).
// Effects work in real coordinates (metres) on the LEDs that exist, never on a dense grid.
#include <Arduino.h>

#include "artnet.h"
#include "config.h"
#include "effects.h"
#include "engine.h"
#include "link_clock.h"
#include "net.h"
#include "osc.h"
#include "palette.h"
#include "store.h"
#include "web.h"

static CRGB leds[MAX_LEDS];
static CRGB lutWrap[256], lutLin[256];
static uint8_t outLut[256];
static float effState[MAXP];
static float motionAcc[MOTIONS];                    // time-based motions: cycles so far
static float motionNow[MOTIONS], motionRate[MOTIONS];  // for the page: phase now, cycles per second
static uint32_t motionCycles[MOTIONS], motionCycle[MOTIONS];  // whole cycles so far (time-based); the cycle now
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

static SemaphoreHandle_t engineMutex = nullptr;  // created first thing in setup()
void engineLock() { xSemaphoreTakeRecursive(engineMutex, portMAX_DELAY); }
void engineUnlock() { xSemaphoreGiveRecursive(engineMutex); }

static void activate(int i) {
    settings.current = i;
    memset(effState, 0, sizeof(effState));
    memset(motionAcc, 0, sizeof(motionAcc));
    memset(motionCycles, 0, sizeof(motionCycles));
    storeMarkDirty();
    Serial.printf("Preset %d: %s\n", i + 1, presets[i].name);
}

void engineSelect(int i) {
    if (i < 0 || i >= presetCount) return;
    isOn = true;
    pending = i != settings.current ? i : -1;
}

// Editing the preset list: the same preset keeps playing wherever it moves to
bool engineDeletePreset(int i) {
    if (!presetDelete(i)) return false;
    if (pending == i) pending = -1;
    else if (pending > i) pending--;
    if (settings.current > i) {
        settings.current--;
    } else if (settings.current == i) {  // the one playing is gone: its neighbour takes over
        if (settings.current >= presetCount) settings.current = presetCount - 1;
        memset(effState, 0, sizeof(effState));
    }
    return true;
}

bool engineMovePreset(int from, int to) {
    if (!presetMove(from, to)) return false;
    auto moved = [&](int k) {
        if (k == from) return to;
        if (from < to && k > from && k <= to) return k - 1;
        if (to < from && k >= to && k < from) return k + 1;
        return k;
    };
    settings.current = moved(settings.current);
    if (pending >= 0) pending = moved(pending);
    return true;
}

void engineSetOn(bool on) { isOn = on; }
bool engineOn() { return isOn; }
void engineResetEffect() { memset(effState, 0, sizeof(effState)); }
float engineFps() { return fps; }
float engineRenderMs() { return renderMs; }
float engineSendMs() { return artnet.sendMs(); }
int engineCurrent() { return pending >= 0 ? pending : settings.current; }
const BeatTime& engineBeat() { return beat; }
void engineMotion(int m, float& phase, float& rate, uint32_t& cycle) {
    const bool ok = m >= 0 && m < MOTIONS;
    phase = ok ? motionNow[m] : 0;
    rate = ok ? motionRate[m] : 0;
    cycle = ok ? motionCycle[m] : 0;
}

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
    const Palette& pal = palettes[p.palette < paletteCount ? p.palette : 0];
    // The palette tables are rebuilt only when the palette changes
    static Palette built;
    static bool have = false;
    if (!have || memcmp(&built, &pal, sizeof(pal)) != 0) {
        buildLut(pal, lutWrap, true);
        buildLut(pal, lutLin, false);
        built = pal;
        have = true;
    }
    const EffectDef& e = EFFECTS[p.effect < NUM_EFFECTS ? p.effect : 0];
    // Following the tempo (or in beats), the other motion runs at bpm/120 times the set speed
    const float edt = y.timing != TIMING_TIME ? dt * (float)beat.bpm / 120.f : dt;
    const float cycleBeats = y.timing == TIMING_BEATS && e.cycle ? (float)(1 << y.beatsExp) : 0.f;
    // Motions move sliders back and forth: the effect gets the moved values, the preset keeps its own
    float pp[MAXP];
    memcpy(pp, p.p, sizeof(pp));
    for (int m = 0; m < MOTIONS; m++) {
        const Motion& mo = presetMotion[settings.current][m];
        motionRate[m] = 0;
        if (!mo.target || mo.target > e.n) continue;
        float ph;
        uint32_t cycle;
        if (mo.inBeats) {  // on the beat grid
            const double per = 1 << mo.beatsExp, x = beat.beat / per + mo.phase;
            ph = (float)(x - floor(x));
            cycle = (uint32_t)(int64_t)floor(x);
            motionRate[m] = (float)(beat.bpm / 60.0 / per);
        } else {  // in time (following the tempo when the preset does)
            motionAcc[m] += edt / mo.seconds;
            const float whole = floorf(motionAcc[m]);
            motionCycles[m] += (uint32_t)whole;
            motionAcc[m] -= whole;
            ph = motionAcc[m] + mo.phase;
            cycle = motionCycles[m] + (uint32_t)floorf(ph);
            ph -= floorf(ph);
            motionRate[m] = (dt > 0 ? edt / dt : 1.f) / mo.seconds;
        }
        motionNow[m] = ph;
        motionCycle[m] = cycle;
        const ParamDef& d = e.params[mo.target - 1];
        float v = mo.from + (mo.to - mo.from) * motionShape(mo.shape, ph, cycle, m);
        v = constrain(v, d.min, d.max);
        if (d.step >= 1) v = roundf(v);
        pp[mo.target - 1] = v;
    }
    Ctx c{pp, edt, effState, lutWrap, lutLin, leds, beat.beat, (float)beat.bpm, cycleBeats, y.reverse != 0};
    e.render(c);
    if (!e.ownSparkle && p.sparkle > 0) sparkleOverlay(leds, p.sparkle, dt);
}

// Gamma, brightness and fade in one table. The fade is squared so it looks even to the eye.
static void buildOutLut() {
    const float B = settings.brightness / 255.f * level * level * beatPulse();
    // The gamma curve (256 powf) only when gamma changes; brightness and fade every frame
    static float gamma = -1, curve[256];
    if (settings.gamma != gamma) {
        gamma = settings.gamma;
        for (int v = 0; v < 256; v++) curve[v] = powf(v / 255.f, gamma) * 255.f;
    }
    for (int v = 0; v < 256; v++) outLut[v] = (uint8_t)(curve[v] * B + 0.5f);
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
            else engineSelect(((pending >= 0 ? pending : settings.current) + 1) % presetCount);
        }
    }
    if (down && !longDone && t - downAt >= LONG_PRESS_MS) {
        longDone = true;
        engineSetOn(!isOn);
    }
}

// ---------------------------------------------------------------- main

void setup() {
    engineMutex = xSemaphoreCreateRecursiveMutex();
    Serial.begin(115200);
    Serial.println("\nbeamLED starting");
    pinMode(cfg::BUTTON_PIN, INPUT_PULLUP);
    storeLoad();
    netLoad();
    effectsInit();
    netBegin();
    linkClock = new LinkClock(linkSettings.bpm);
    artnet.begin();
    webBegin();
    printLayout();
    Serial.printf("Preset %d, Art-Net to %s\n", settings.current + 1,
                  IPAddress(settings.target).toString().c_str());
}

void loop() {
    netLoop();
    webHandle();
    {
        EngineLock lock;
        pollButton();
        storePoll();
    }
    oscLoop();  // takes the lock itself

    // Between frames the core already idles: webHandle() sleeps 1 ms while no browser is
    // connected (WebServer's null delay). Frames keep to a steady 1/FPS grid, so that sleep
    // making one frame a little late doesn't slow the rate; dt is the real time between frames.
    static const uint32_t PERIOD = 1000000u / cfg::FPS;
    static uint32_t due = micros(), prev = micros();
    const uint32_t now = micros();
    if ((int32_t)(now - due) < 0) return;
    due = now - due > 2 * PERIOD ? now + PERIOD : due + PERIOD;  // fell far behind: start afresh
    const float dt = fminf((now - prev) / 1e6f, 0.1f);
    prev = now;

    uint32_t t0, t1;
    {
        EngineLock lock;  // the web API waits for the frame, not the other way round
        updateLink();
        updateFade(dt);
        t0 = micros();
        if (netOtaActive()) return;  // keep the network free for the update
        if (level > 0) renderFrame(dt);
        else memset(leds, 0, layoutLeds() * sizeof(CRGB));
        buildOutLut();
        t1 = micros();
        if (netCanSend()) artnet.send(IPAddress(settings.target), leds, outLut);  // handed to its task
    }

    // Stats, smoothed
    renderMs += ((t1 - t0) / 1000.f - renderMs) * 0.05f;
    fps += (1.f / fmaxf(dt, 0.001f) - fps) * 0.05f;
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 10000) {
        lastLog = millis();
        Serial.printf("%.1f fps, render %.1f ms, heap %u\n", fps, renderMs, ESP.getFreeHeap());
    }
}
