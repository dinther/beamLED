// beamLED: standalone LED bar renderer (WT32-ETH01 -> Art-Net -> pixel controller).
// Effects work in real coordinates (metres) on the LEDs that exist, never on a dense grid.
#include <atomic>
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

static LinkClock* linkClock = nullptr;  // created when Link is first enabled (never while disabled)
static BeatTime beat;              // where the music is, this frame
static uint32_t linkAloneSince = 0;
static int linkPeers = 0;

static const float FADE_SWITCH_S = 0.4f;  // each way when changing preset
static const float FADE_ONOFF_S = 1.0f;
static const uint32_t LONG_PRESS_MS = 700;
static const uint32_t DOUBLE_MS = 400;  // a second press within this of the first: double press

// ---------------------------------------------------------------- engine

static SemaphoreHandle_t engineMutex = nullptr;  // created first thing in setup()
// The render loop holds the lock for most of each frame and takes it again microseconds after
// letting go. The web server waiting for it runs on the other core, often busy with Art-Net just
// then, so it could lose that race every time: with 6000 LEDs the page and the API stopped
// answering altogether. So the render loop lets anyone waiting go first (up to 20 ms).
static TaskHandle_t renderTask = nullptr;  // the loop's task (setup() runs on it)
static std::atomic<int> lockWaiters{0};    // other tasks waiting for the lock
void engineLock() {
    if (xTaskGetCurrentTaskHandle() == renderTask) {
        for (int t = 0; lockWaiters.load() > 0 && t < 20; t++) vTaskDelay(1);
        xSemaphoreTakeRecursive(engineMutex, portMAX_DELAY);
        return;
    }
    lockWaiters++;
    xSemaphoreTakeRecursive(engineMutex, portMAX_DELAY);
    lockWaiters--;
}
void engineUnlock() { xSemaphoreGiveRecursive(engineMutex); }

static bool pendingByAuto = false;  // the change under way is Auto's: not one to remember

static void activate(int i) {
    settings.current = i;
    memset(effState, 0, sizeof(effState));
    memset(motionAcc, 0, sizeof(motionAcc));
    memset(motionCycles, 0, sizeof(motionCycles));
    // Remembered for a power cut when picked by hand; Auto's changes are not saved (each save would
    // wear the flash, and Auto carries on from the preset picked last anyway)
    if (!pendingByAuto) storeMarkDirty();
    pendingByAuto = false;
    Serial.printf("Preset %d: %s\n", i + 1, presets[i].name);
}

// Auto: how far it has come since the last change (any change, by hand too, starts it again)
static float autoAcc = 0;          // seconds
static double autoSlot = -1;       // bars: the stretch of N bars we are in (-1: not known yet)
static float autoLeft = -1;        // until the next change, in seconds or bars (-1: not running)

void engineSelect(int i) {
    if (i < 0 || i >= presetCount) return;
    pendingByAuto = false;  // by hand (Auto sets it again right after, for its own)
    autoAcc = 0;
    autoSlot = -1;
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
float engineAutoLeft() { return autoLeft; }

// The next preset for Auto: the next one in it, or a random other one (shuffle). -1: none to go to.
static int autoPick() {
    const int cur = pending >= 0 ? pending : settings.current;
    int in[MAX_PRESETS], n = 0;
    for (int i = 0; i < presetCount; i++)
        if (presetInAuto(i) && i != cur) in[n++] = i;
    if (!n) return -1;
    if (autoSettings.shuffle) return in[esp_random() % n];
    for (int k = 1; k <= presetCount; k++) {
        const int i = (cur + k) % presetCount;
        if (presetInAuto(i) && i != cur) return i;
    }
    return -1;
}

// Every frame: change preset when the time is up, or when the beat crosses the next multiple of N bars
static void updateAuto(float dt) {
    const AutoSettings& a = autoSettings;
    if (!a.enabled || !isOn || pending >= 0) {
        autoAcc = 0;
        autoSlot = -1;
        autoLeft = -1;
        return;
    }
    bool due = false;
    if (a.inBars) {
        const double every = 1 << a.barsExp, bars = beat.beat / (beat.quantum > 0 ? beat.quantum : 4);
        const double slot = floor(bars / every);
        if (autoSlot < 0) autoSlot = slot;
        else if (slot != autoSlot) due = true;
        autoLeft = (float)((slot + 1) * every - bars);
    } else {
        autoAcc += dt;
        due = autoAcc >= a.seconds;
        autoLeft = a.seconds - autoAcc;
    }
    if (!due) return;
    const int next = autoPick();
    if (next >= 0) {
        engineSelect(next);  // also starts the count again
        pendingByAuto = true;
    }
    else autoAcc = 0, autoSlot = -1;
}
bool engineOn() { return isOn; }
void engineResetEffect() { memset(effState, 0, sizeof(effState)); }
float engineFps() { return fps; }
float engineRenderMs() { return renderMs; }
float engineSendMs() { return artnet.sendMs(); }
void engineArtNetStats(uint32_t& sent, uint32_t& failed, int& err, uint32_t& skipped) {
    sent = artnet.sent;
    failed = artnet.failed;
    err = artnet.lastErrno;
    skipped = artnet.skipped;
}
void engineArtNetTiming(uint32_t& dropped, uint32_t& waitMaxUs, uint32_t& sendMaxUs) {
    dropped = artnet.dropped;
    artnet.takeTiming(waitMaxUs, sendMaxUs);
}
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
    // A Link that is never enabled is never created: no threads, no sockets, nothing on the
    // hotspot. Until then the board keeps its own beat at its own tempo.
    if (!linkClock && linkSettings.enabled && !netConfigMode()) {  // no Link in config mode
        const double from = beat.beat;
        linkClock = new LinkClock(linkSettings.bpm);
        linkClock->setBeat(from);
    }
    if (!linkClock) {
        static uint32_t prevUs = micros();
        const uint32_t nowUs = micros();
        beat.bpm = linkSettings.bpm;
        beat.beat += (nowUs - prevUs) * 1e-6 * linkSettings.bpm / 60.0;
        prevUs = nowUs;
        beat.quantum = 4;
        beat.phase = fmod(beat.beat, beat.quantum);
        beat.peers = 0;
        beat.link = false;
        return;
    }
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
    // The palette lock, when on, overrides each preset's own palette (without changing it)
    const int pk = paletteLock >= 0 && paletteLock < paletteCount ? paletteLock : p.palette;
    const Palette& pal = palettes[pk < paletteCount ? pk : 0];
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
    effectsToUnits(e, pp);  // lengths and speeds from % of the layout's shorter side to its units
    Ctx c{pp, edt, effState, lutWrap, lutLin, leds, beat.beat, (float)beat.bpm, cycleBeats, y.reverse != 0,
          presetSteps[settings.current].lane, levels.effect / 255.f};
    effectsRender(e, c, (p.mirror & 1) != 0);  // Mirror Y: only the first half of each row rendered
    if (p.mirror & 2) {  // Mirror X: the last rows the mirror image of the first
        const int nx = layout.nx, ny = layout.ny;
        for (int b = 0; b < ny / 2; b++) memcpy(leds + (ny - 1 - b) * nx, leds + b * nx, nx * sizeof(CRGB));
    }
    // Effect brightness: the effect alone, before the overlay's twinkles go on top (an effect that
    // applies it itself, Strobe, has done so)
    if (levels.effect < 255 && !e.ownLevel) {
        const int L = levels.effect, n = layoutLeds();
        for (int i = 0; i < n; i++) leds[i] = CRGB(leds[i].r * L / 255, leds[i].g * L / 255, leds[i].b * L / 255);
    }
    // The Sparkle overlay, the same over every preset (not on the Sparkle effect itself)
    const SparkleSettings& sk = sparkleSettings;
    if (!e.ownSparkle && sk.amount > 0) {
        // Its colours: white, the playing preset's palette, or another palette (its table on the
        // heap, rebuilt only when that palette changes)
        // Through the palette: the same palette as a ramp (first colour to last), not a loop
        const bool through = (sk.beat & SPARKLE_THROUGH) != 0 && sk.colour >= 1;
        const CRGB* lut = nullptr;
        const CRGB* ramp = nullptr;
        if (sk.colour == 1) {
            lut = lutWrap;
            if (through) ramp = lutLin;
        } else if (sk.colour >= 2 && sk.colour - 2 < paletteCount) {
            static CRGB* spLut = nullptr;     // loop
            static CRGB* spLutLin = nullptr;  // ramp
            static Palette* spBuilt = nullptr;
            static bool spHave = false;
            if (!spLut) spLut = (CRGB*)malloc(256 * sizeof(CRGB));
            if (!spLutLin) spLutLin = (CRGB*)malloc(256 * sizeof(CRGB));
            if (!spBuilt) spBuilt = (Palette*)calloc(1, sizeof(Palette));
            const Palette& sp = palettes[sk.colour - 2];
            if (spLut && spLutLin && spBuilt) {
                if (!spHave || memcmp(spBuilt, &sp, sizeof(sp)) != 0) {
                    buildLut(sp, spLut, true);
                    buildLut(sp, spLutLin, false);
                    *spBuilt = sp;
                    spHave = true;
                }
                lut = spLut;
                if (through) ramp = spLutLin;
            }
        }
        // Ticks per beat by timing code: 1..4 = every 1, 1/2, 1/4, 1/8 beat; 5 = every 2 beats, 6 = every 4
        static const float DIV[7] = {0, 1, 2, 4, 8, 0.5f, 0.25f};
        const int grid = sk.beat & SPARKLE_GRID;
        const float div = grid <= 6 ? DIV[grid] : 0;
        sparkleOverlay(leds, sk.amount, dt, lut, beat.beat, (float)beat.bpm, div, (sk.beat & SPARKLE_BURST) != 0, sk.colourStep,
                       ramp, sk.life, levels.sparkle);
    }
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

// Short press: Auto off, and next preset (or on). Double press: Auto on (and the lights on).
// Long press: on/off. A short press waits DOUBLE_MS to see whether a second one follows.
static void pollButton() {
    static bool down = false, longDone = false;
    static uint32_t changedAt = 0, downAt = 0, shortAt = 0;
    static bool shortWaiting = false;
    const bool now = digitalRead(cfg::BUTTON_PIN) == LOW;
    const uint32_t t = millis();
    static bool armed = false;  // a press only counts once the button was up: holding it at
    if (!armed) {               // power-on (config mode) is not a long press
        armed = !now;
        return;
    }
    if (now != down && t - changedAt > 30) {  // debounce
        changedAt = t;
        down = now;
        if (down) {
            downAt = t;
            longDone = false;
        } else if (!longDone) {  // a short press
            if (shortWaiting && t - shortAt <= DOUBLE_MS) {  // the second one: double press
                shortWaiting = false;
                engineSetOn(true);
                if (!autoSettings.enabled) {
                    autoSettings.enabled = 1;
                    storeMarkDirty();
                    webChanged();
                    Serial.println("Auto on");
                }
            } else {
                shortWaiting = true;
                shortAt = t;
            }
        }
    }
    if (shortWaiting && !down && t - shortAt > DOUBLE_MS) {  // no second press came: a single one
        shortWaiting = false;
        if (autoSettings.enabled) {  // picking by hand ends Auto
            autoSettings.enabled = 0;
            storeMarkDirty();
            webChanged();
            Serial.println("Auto off");
        }
        if (!isOn) engineSetOn(true);
        else engineSelect(((pending >= 0 ? pending : settings.current) + 1) % presetCount);
    }
    if (down && !longDone && t - downAt >= LONG_PRESS_MS) {
        longDone = true;
        shortWaiting = false;  // short then long: just the long press
        engineSetOn(!isOn);
    }
}

// ---------------------------------------------------------------- main

void setup() {
    engineMutex = xSemaphoreCreateRecursiveMutex();
    renderTask = xTaskGetCurrentTaskHandle();
    Serial.begin(115200);
    Serial.println("\nbeamLED starting");
    pinMode(cfg::BUTTON_PIN, INPUT_PULLUP);
    delay(20);  // let the pull-up settle
    // The button held at power-on: config mode (hotspot and web page only, no Art-Net)
    const bool config = digitalRead(cfg::BUTTON_PIN) == LOW;
    storeLoad();
    netLoad();
    effectsInit();
    netBegin(config);
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
        updateAuto(dt);
        updateFade(dt);
        t0 = micros();
        if (netOtaActive()) return;  // keep the network free for the update
        if (level > 0) renderFrame(dt);
        else memset(leds, 0, layoutLeds() * sizeof(CRGB));
        // The page's live preview: the colours as rendered, dimmed as the output is (no gamma)
        if (oscPreviewWanted())
            oscPreviewFrame(leds, layout.nx, layout.ny, settings.brightness / 255.f * level * level * beatPulse());
        buildOutLut();
        t1 = micros();
        const IPAddress to(settings.target);
        if (netCanSendTo(to)) artnet.send(to, leds, outLut);  // handed to its task
        else if (netCanSend()) artnet.skipped++;
    }

    // A frame that ran into the next one (a big layout, a heavy effect): give way for a moment, or
    // the loop takes the engine lock again at once and the web server (on the other core, waiting
    // for it) never gets in: with 6000 LEDs the page and the API stopped answering altogether
    if ((int32_t)(micros() - due) >= 0) vTaskDelay(1);

    // Stats, smoothed
    renderMs += ((t1 - t0) / 1000.f - renderMs) * 0.05f;
    fps += (1.f / fmaxf(dt, 0.001f) - fps) * 0.05f;
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 10000) {
        lastLog = millis();
        Serial.printf("%.1f fps, render %.1f ms, heap %u\n", fps, renderMs, ESP.getFreeHeap());
    }
}
