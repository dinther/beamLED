#pragma GCC optimize("O2")  // effects are built for speed (Drops: 20% faster); same results
#include "effects.h"

#ifdef ARDUINO
#include <Arduino.h>  // micros, for the benchmark
#endif
#include <math.h>
#include <string.h>

#include "config.h"

static constexpr float DEG = 0.01745329f;
static constexpr float TAU = 6.2831853f;

static float X[MAX_X];   // m along a row
static float Y[MAX_Y];   // m across the rows
static float FY[MAX_Y];  // footprint of a row across the rows (m)
// NX, NL: the LEDs per row and in all that an effect renders (in mirror, the left half only)
static int NX = 1, NY = 1, NL = 1;
static float CX, CY, W, H;
static float PITCH = 1;  // m from one LED to the next along a row
static int NXFULL = 1;   // LEDs per row in the layout
static float noiseScale = 1.f / 15000.f;

// ---------------------------------------------------------------- helpers

static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }
// floorf without the library call (exact for |x| < 2^31)
static inline int floorI(float x) {
    const int n = (int)x;
    return x < n ? n - 1 : n;
}

static inline float sstep(float e0, float e1, float x) {
    float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3 - 2 * t);
}

static inline CRGB lutW(const CRGB* l, float pos) { return l[floorI(pos * 256.f) & 255]; }

static inline CRGB lutL(const CRGB* l, float pos) {
    int i = (int)(pos * 255.f + 0.5f);
    return l[i < 0 ? 0 : (i > 255 ? 255 : i)];
}

static inline CRGB scaleF(const CRGB& c, float k) {
    k = clamp01(k);
    return CRGB((uint8_t)(c.r * k + 0.5f), (uint8_t)(c.g * k + 0.5f), (uint8_t)(c.b * k + 0.5f));
}

static inline CRGB mixF(const CRGB& a, const CRGB& b, float t) {
    t = clamp01(t);
    return CRGB((uint8_t)(a.r + (b.r - a.r) * t + 0.5f), (uint8_t)(a.g + (b.g - a.g) * t + 0.5f),
                (uint8_t)(a.b + (b.b - a.b) * t + 0.5f));
}

static inline float wrap256(float a) {
    a = fmodf(a, 256.f);
    return a < 0 ? a + 256.f : a;
}

static inline float frand() { return random16() / 65535.f; }

// Fast maths. The ESP32's FPU has no divide, square root, exp or trig in hardware, and the
// effects run them for each of the 2700 LEDs. Tables with linear interpolation, accurate to
// about 3e-5 (one step of 8-bit colour is 4e-3).
static float cosTab[1025];  // cos(TAU * k / 1024)
static constexpr float EXP_MIN = -8.f;
static float expTab[513];   // exp(x), x from EXP_MIN to 0 in 512 steps
static void fastInit() {
    for (int k = 0; k <= 1024; k++) cosTab[k] = cosf(TAU * k / 1024.f);
    for (int k = 0; k <= 512; k++) expTab[k] = expf(EXP_MIN * (1.f - k / 512.f));
}
// cos(TAU * u), u in cycles
static inline float cosCyc(float u) {
    float f = (u - floorI(u)) * 1024.f;
    int k = (int)f;
    if (k > 1023) k = 1023;
    f -= k;
    return cosTab[k] + (cosTab[k + 1] - cosTab[k]) * f;
}
// exp(x) for x <= 0; 0 below EXP_MIN (where exp is under 4e-4)
static inline float expNeg(float x) {
    if (x <= EXP_MIN) return 0;
    if (x >= 0) return 1;
    float f = (x - EXP_MIN) * (512.f / -EXP_MIN);
    int k = (int)f;
    if (k > 511) k = 511;
    f -= k;
    return expTab[k] + (expTab[k + 1] - expTab[k]) * f;
}
// 1/sqrt(x) for x > 0: the bit trick plus two Newton steps, relative error under 5e-6
static inline float rsqrtFast(float x) {
    union { float f; uint32_t i; } u{x};
    u.i = 0x5f3759df - (u.i >> 1);
    float y = u.f;
    const float h = 0.5f * x;
    y = y * (1.5f - h * y * y);
    return y * (1.5f - h * y * y);
}
// atan2, max error about 1e-5 rad
static inline float atan2Fast(float y, float x) {
    const float ax = fabsf(x), ay = fabsf(y), mx = fmaxf(ax, ay);
    if (mx == 0) return 0;
    const float z = fminf(ax, ay) / mx, z2 = z * z;
    float a = z * (0.99997726f + z2 * (-0.33262347f + z2 * (0.19354346f + z2 * (-0.11643287f +
                   z2 * (0.05265332f + z2 * -0.01172120f)))));
    if (ay > ax) a = 1.57079633f - a;
    if (x < 0) a = 3.14159265f - a;
    return y < 0 ? -a : a;
}
static inline float roundFast(float x) { return (float)(int)(x + (x < 0 ? -0.5f : 0.5f)); }
// smoothstep of t already scaled to the edge: (x - e0) / (e1 - e0)
static inline float sm(float t) {
    t = clamp01(t);
    return t * t * (3 - 2 * t);
}

// Moves an effect's main cycle: `acc` advances `rate` per second, one cycle = `units`.
// Timing in beats: a cycle takes c.cycleBeats beats instead, and the cycle starts ease onto the
// beat grid (onto bars for cycles of 4+ beats) within a few seconds, without jumps.
// Uses state slots 5 and 6.
static void advance(Ctx& c, float& acc, float rate, float units) {
    if (!(c.cycleBeats > 0) || !(units > 0)) {
        acc += rate * c.dt;
        c.s[6] = 0;
        return;
    }
    const float dir = c.reverse ? -1.f : 1.f;
    const double bpc = c.cycleBeats;
    const double base = dir * c.beat / bpc * units;
    if (c.s[6] != (float)(bpc * dir)) {  // (re)locking or a new speed: carry on from here
        c.s[5] = (float)(acc - base);
        c.s[6] = (float)(bpc * dir);
    }
    const float aligned = roundf(c.s[5] / units) * units;
    c.s[5] += (aligned - c.s[5]) * fminf(1.f, c.dt * 0.5f);
    acc = (float)(base + c.s[5]);
}

// Soft bands. u in band units (band n centred on u = n, borders at n + 0.5); k = width of the
// colour transition as a fraction of a band; step = palette advance per band; gap = how dark
// the borders get.
static inline CRGB bands(float u, float k, float step, float gap, const CRGB* lut) {
    k = fminf(1.f, fmaxf(0.03f, k));
    const float n = floorf(u), f = u - n;
    const float cpos = (n + sstep(0.5f - k * 0.5f, 0.5f + k * 0.5f, f)) * step;
    const float w = 1.f - sstep(0.f, k * 0.5f + 0.001f, fabsf(f - 0.5f));
    return scaleF(lutW(lut, cpos), 1.f - gap * w);
}

// The same bands with k fixed (per row, say): the divisions are done once
// hg: half the width of a dark core at each border, in band units (0 = none: the gaps are only the
// colour transitions)
struct Bands {
    float lo, rk, re, step, gap, hg;
    Bands(float k, float step_, float gap_, float hg_ = 0) : step(step_), gap(gap_), hg(hg_) {
        k = fminf(1.f, fmaxf(0.03f, k));
        lo = 0.5f - k * 0.5f;
        rk = 1.f / k;
        re = 1.f / (k * 0.5f + 0.001f);
    }
    CRGB at(float u, const CRGB* lut) const {
        const int n = floorI(u);
        const float f = u - n;
        const float cpos = (n + sm((f - lo) * rk)) * step;
        const float w = 1.f - sm((fabsf(f - 0.5f) - hg) * re);
        return scaleF(lutW(lut, cpos), 1.f - gap * w);
    }
};

// Contrast left of a cosine of the given period after averaging over a row's footprint, for
// a pattern whose gradient makes sinAbs with the rows (box filter ~ Gaussian, sd = FY/sqrt(12))
static inline float footAtten(int b, float sinAbs, float period) {
    const float a = FY[b] * sinAbs / period;
    return expf(-1.645f * a * a);
}

// Fractal noise, roughly -1..1. Octaves finer than 0.8 m are left out.
static float fbm(float x, float y, float z, float size) {
    float sum = 0, tw = 0, w = 1, lam = size;
    for (int o = 0; o < 4; o++) {
        if (o > 0 && lam < 0.8f) break;
        const float u = wrap256(x / lam + o * 17.3f);
        const float v = wrap256(y / lam + o * 31.7f);
        const float t = wrap256(z * (1 + o * 0.7f) + o * 47.1f);
        const int n = (int)inoise16((uint32_t)(u * 65536.f), (uint32_t)(v * 65536.f),
                                    (uint32_t)(t * 65536.f)) - 32768;
        sum += n * w;
        tw += w;
        w *= 0.5f;
        lam *= 0.5f;
    }
    return sum / tw * noiseScale;
}

// ---------------------------------------------------------------- sparkle engine

static uint16_t spPhase[MAX_LEDS];  // 0 = idle, else 1..65535 through one twinkle
static uint8_t spColour[MAX_LEDS];

static void sparkleStep(float density, float life, float dt) {
    // density 1 = about 8% of the LEDs lit at any moment
    const float prob = density * 0.08f / life * dt;
    const uint16_t th = (uint16_t)fminf(prob * 65535.f, 65535.f);
    const uint32_t inc = (uint32_t)fmaxf(1.f, 65535.f * dt / life);
    for (int i = 0; i < NL; i++) {
        if (spPhase[i]) {
            uint32_t n = spPhase[i] + inc;
            spPhase[i] = n >= 65535 ? 0 : n;
        } else if (th && random16() < th) {
            spPhase[i] = 1;
            spColour[i] = random8();
        }
    }
}

// Quick rise, slow fall
static inline float sparkleLevel(uint16_t p) {
    if (!p) return 0;
    float t = p / 65535.f;
    if (t < 0.15f) return t / 0.15f;
    float d = (1 - t) / 0.85f;
    return d * d;
}

// Bursts: a flash at once, then a quick fade
static inline float burstLevel(uint16_t p) {
    if (!p) return 0;
    const float d = 1.f - p / 65535.f;
    return d * d;
}

// On the beat: on each tick (div per beat) a batch of random LEDs lights up; each fades out within
// 90% of a tick. density 1 = 6% of the LEDs per burst.
static void burstStep(float density, double beat, float bpm, int div, float dt) {
    static int64_t lastTick = INT64_MIN;
    const int64_t tick = (int64_t)floor(beat * div);
    const float life = 0.9f * 60.f / (fmaxf(bpm, 20.f) * div);
    const uint32_t inc = (uint32_t)fmaxf(1.f, 65535.f * dt / life);
    for (int i = 0; i < NL; i++) {
        if (!spPhase[i]) continue;
        const uint32_t n = spPhase[i] + inc;
        spPhase[i] = n >= 65535 ? 0 : n;
    }
    if (tick == lastTick) return;
    const bool first = lastTick == INT64_MIN || tick < lastTick || tick - lastTick > 64;
    lastTick = tick;
    if (first) return;  // just started (or the beat jumped): wait for the next tick
    const int n = (int)(density * 0.06f * NL + 0.5f);
    for (int k = 0; k < n; k++) {
        const int i = random16() % NL;
        spPhase[i] = 1;
        spColour[i] = random8();
    }
}

// Twinkles as in free sparkle, but each starts on a tick: the ones that would have started at
// random since the last tick all start on it
static void gridStep(float density, double beat, int div, float dt) {
    const float life = 1.2f;
    static int64_t lastTick = INT64_MIN;
    static float due = 0;  // twinkles owed since the last tick
    const uint32_t inc = (uint32_t)fmaxf(1.f, 65535.f * dt / life);
    for (int i = 0; i < NL; i++) {
        if (!spPhase[i]) continue;
        const uint32_t n = spPhase[i] + inc;
        spPhase[i] = n >= 65535 ? 0 : n;
    }
    due += density * 0.08f / life * dt * NL;  // what free sparkle starts on average (on dark LEDs)
    const int64_t tick = (int64_t)floor(beat * div);
    if (tick == lastTick) return;
    const bool first = lastTick == INT64_MIN || tick < lastTick || tick - lastTick > 64;
    lastTick = tick;
    if (first) { due = 0; return; }  // just started (or the beat jumped): from the next tick on
    const int n = (int)due;
    due -= n;
    for (int k = 0; k < n; k++) {
        const int i = random16() % NL;
        if (spPhase[i]) continue;  // already twinkling (free sparkle only starts dark ones too)
        spPhase[i] = 1;
        spColour[i] = random8();
    }
}

void sparkleOverlay(CRGB* out, float density, float dt, const CRGB* lut, double beat, float bpm, int div,
                    bool burst) {
    burst = burst && div > 0;
    if (burst) burstStep(density, beat, bpm, div, dt);
    else if (div > 0) gridStep(density, beat, div, dt);
    else sparkleStep(density, 1.2f, dt);
    for (int i = 0; i < NL; i++) {
        if (!spPhase[i]) continue;
        uint8_t a = (uint8_t)((burst ? burstLevel(spPhase[i]) : sparkleLevel(spPhase[i])) * 255.f);
        if (!lut) {
            out[i] += CRGB(a, a, a);
            continue;
        }
        const CRGB& k = lut[spColour[i]];
        out[i] += CRGB(k.r * a / 255, k.g * a / 255, k.b * a / 255);
    }
}

// ---------------------------------------------------------------- Rolling bars
// Wide bands rolling across the grid in the direction Angle sets.

static void rolling(Ctx& c) {
    const float speed = c.p[0], width = c.p[1], soft = c.p[2], gap = c.p[3], angle = c.p[4] * DEG,
                step = c.p[5], gapW = c.p[7];
    advance(c, c.s[1], speed / width, 1.f);  // in beats: one band per cycle
    const float ca = cosf(angle), sa = sinf(angle), iw = 1.f / width;
    const float ph = c.s[1] + c.p[6] * iw;  // where the bands are: their motion plus Offset
    for (int b = 0; b < NY; b++) {
        // Transition at least the row's footprint projected onto the roll direction
        const Bands bd(fmaxf(soft, FY[b] * fabsf(sa) * iw), step, gap, gapW * 0.5f);
        const float dy = (Y[b] - CY) * sa;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) o[i] = bd.at(((X[i] - CX) * ca + dy) * iw - ph, c.lutWrap);
    }
}

// ---------------------------------------------------------------- Sweeps
// Bars sweeping across the grid one after another, each pass turned on by Angle change from the
// last, with a trail. Angle turns them all.

static float sweepAng[4], sweepPrev[4];  // each bar's direction before Angle; where it is
static float sweepAt[4];                 // Angle as it was when the bar's pass began
static uint8_t sweepCol[4];

static void sweeps(Ctx& c) {
    const int n = clampi((int)(c.p[0] + 0.5f), 1, 4);
    const float speed = c.p[1], width = c.p[2], trail = c.p[3], change = c.p[4], angle = c.p[5] * DEG,
                bg = c.p[6], soft = c.p[7];
    if (c.s[7] == 0) {
        c.s[7] = 1;
        for (int k = 0; k < 4; k++) {
            sweepAng[k] = k * 1.3f;
            sweepCol[k] = k * 64;
            sweepPrev[k] = 1e9f;
            sweepAt[k] = angle;
        }
    }
    const float L = hypotf(W, H) + 2 * width + 2 * trail + 1.f;  // full travel
    advance(c, c.s[0], speed, L / n);  // in beats: a new bar per cycle
    float pos[4], ca[4], sa[4];
    CRGB col[4];
    for (int k = 0; k < n; k++) {
        float p = fmodf(c.s[0] + k * L / n, L);
        if (p < 0) p += L;
        // wrapped (not just eased back a little by the beat lock): next pass, new angle
        if (p < sweepPrev[k] - L * 0.5f && sweepPrev[k] < 1e8f) {
            sweepAng[k] += change * DEG;
            sweepAt[k] = angle;  // Angle is taken at the start of a pass: a change never turns a bar mid-pass
            sweepCol[k] += 51;
        }
        sweepPrev[k] = p;
        pos[k] = p - L * 0.5f;
        ca[k] = cosf(sweepAng[k] + sweepAt[k]);
        sa[k] = sinf(sweepAng[k] + sweepAt[k]);
        col[k] = c.lutWrap[sweepCol[k]];
    }
    const CRGB back = c.lutLin[0];
    const float hw = width * 0.5f, itrail = trail > 0.01f ? 1.f / trail : 0;
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        float ie[4];  // 1 / edge width
        for (int k = 0; k < n; k++) ie[k] = 1.f / fmaxf(fmaxf(soft * width, FY[b] * fabsf(sa[k])), 0.05f);
        for (int i = 0; i < NX; i++) {
            float r = back.r * bg, g = back.g * bg, bl = back.b * bg;
            for (int k = 0; k < n; k++) {
                // s > 0 ahead of the bar's centre, s < 0 behind it
                const float s = (X[i] - CX) * ca[k] + (Y[b] - CY) * sa[k] - pos[k];
                float I;
                if (s > hw) I = 1.f - sm((s - hw) * ie[k]);
                else if (s >= -hw) I = 1.f;
                else {
                    I = 1.f - sm((-hw - s) * ie[k]);
                    if (trail > 0.01f) I = fmaxf(I, expNeg((s + hw) * itrail));
                }
                if (I <= 0.002f) continue;
                r += col[k].r * I;
                g += col[k].g * I;
                bl += col[k].b * I;
            }
            o[i] = CRGB((uint8_t)fminf(r, 255.f), (uint8_t)fminf(g, 255.f), (uint8_t)fminf(bl, 255.f));
        }
    }
}

// ---------------------------------------------------------------- Plaid
// Two sets of stripes crossing at an angle, slowly turning; colour by which set dominates.

static void plaid(Ctx& c) {
    const float period = c.p[0], speed = c.p[1], turn = c.p[2], cross = c.p[3], contrast = c.p[4],
                drift = c.p[5];
    c.s[0] += c.dt * turn * DEG;
    advance(c, c.s[1], speed / period, 1.f);
    c.s[2] += c.dt * drift * 0.02f;
    const float a1 = c.s[0], a2 = c.s[0] + cross * DEG;
    const float c1 = cosf(a1), s1 = sinf(a1), c2 = cosf(a2), s2 = sinf(a2), ip = 1.f / period;
    const float ph = c.s[1] + c.p[6] * ip;  // their motion plus Offset
    for (int b = 0; b < NY; b++) {
        const float at1 = footAtten(b, fabsf(s1), period), at2 = footAtten(b, fabsf(s2), period);
        const float dy = Y[b] - CY;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - CX;
            const float v1 = cosCyc((dx * c1 + dy * s1) * ip - ph) * at1;
            const float v2 = cosCyc((dx * c2 + dy * s2) * ip + ph * 0.7f) * at2;
            const CRGB colour = lutW(c.lutWrap, c.s[2] + 0.5f * sstep(-0.5f, 0.5f, v1 - v2));
            const float lvl = 1.f - contrast * (1.f - (v1 + v2 + 2.f) * 0.25f);
            o[i] = scaleF(colour, lvl);
        }
    }
}

// ---------------------------------------------------------------- Searchlights
// Soft round beams wandering over the grid on slow Lissajous paths.

static void searchlights(Ctx& c) {
    const int n = clampi((int)(c.p[0] + 0.5f), 1, 5);
    const float size = c.p[1], speed = c.p[2], bg = c.p[3], soft = c.p[4];
    c.s[0] += c.dt * speed;
    const float t = c.s[0];
    float bx[5], by[5];
    CRGB col[5];
    const float ax = W * 0.5f + 0.3f, ay = H * 0.5f + 0.3f;
    for (int k = 0; k < n; k++) {
        bx[k] = CX + ax * sinf(t * (0.31f + 0.071f * k) + k * 1.7f);
        by[k] = CY + ay * sinf(t * (0.23f + 0.053f * k) + k * 2.9f + 0.5f);
        col[k] = lutW(c.lutWrap, (float)k / n);
    }
    const float inner = fmaxf(0.f, fminf(size * (1.f - soft), size - 0.4f));  // edge >= 0.4 m
    const float size2 = size * size, iedge = 1.f / (size - inner);
    const CRGB back = c.lutWrap[0];
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            float r = back.r * bg, g = back.g * bg, bl = back.b * bg;
            for (int k = 0; k < n; k++) {
                const float dx = X[i] - bx[k], dy = Y[b] - by[k];
                const float d2 = dx * dx + dy * dy;
                if (d2 >= size2) continue;
                const float I = 1.f - sm((sqrtf(d2) - inner) * iedge);
                r += col[k].r * I;
                g += col[k].g * I;
                bl += col[k].b * I;
            }
            o[i] = CRGB((uint8_t)fminf(r, 255.f), (uint8_t)fminf(g, 255.f), (uint8_t)fminf(bl, 255.f));
        }
    }
}

// ---------------------------------------------------------------- Lighthouse
// A beam turning around a point at (or beyond) the edge of the grid.

static void lighthouse(Ctx& c) {
    const float rpm = c.p[0], wdeg = c.p[1];
    const int arms = clampi((int)(c.p[2] + 0.5f), 1, 4);
    const float ox = c.p[3] * W, oy = Y[0] + c.p[4] * H, bg = c.p[5], spread = c.p[6];
    const float P = TAU / arms, iP = arms / TAU, half = wdeg * DEG * 0.5f, angle = c.p[7] * DEG;
    advance(c, c.s[0], rpm * (TAU / 60.f), TAU);  // in beats: one revolution per cycle
    float maxR = 0;
    for (float cx : {0.f, W})
        for (float cy : {Y[0], Y[0] + H}) maxR = fmaxf(maxR, hypotf(cx - ox, cy - oy));
    const CRGB back = scaleF(c.lutLin[0], bg);
    const float sr = spread / maxR;
    // The beam's edge is half * 0.6 wide, but near the origin (where that is narrow in metres)
    // at least ~0.35 m: 0.35 / r. Where the first wins, its smoothstep is the same everywhere.
    const float e0 = half * 0.6f, lo0 = fmaxf(half - e0, 0.f), r0 = 1.f / (half + e0 - lo0);
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        const float dy = Y[b] - oy;
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - ox;
            const float r = sqrtf(dx * dx + dy * dy);
            float a = atan2Fast(dy, dx) - c.s[0] - angle;
            a -= P * roundFast(a * iP);
            const float rc = fmaxf(r, 0.2f);
            float I;
            if (0.35f <= e0 * rc) {
                I = 1.f - sm((fabsf(a) - lo0) * r0);
            } else {
                const float edge = 0.35f / rc, lo = fmaxf(half - edge, 0.f);
                I = 1.f - sm((fabsf(a) - lo) / (half + edge - lo));
            }
            o[i] = mixF(back, lutL(c.lutLin, r * sr), I);
        }
    }
}

// ---------------------------------------------------------------- Rings
// Concentric rings flowing out of (or into) a slowly wandering centre.

static void rings(Ctx& c) {
    const float spacing = c.p[0], speed = c.p[1], soft = c.p[2], gap = c.p[3], wander = c.p[4],
                wspeed = c.p[5], step = c.p[6];
    advance(c, c.s[0], speed / spacing, 1.f);  // in beats: a ring per cycle
    c.s[1] += c.dt * wspeed;
    const float cx = CX + wander * W * 0.5f * sinf(c.s[1] * 0.37f);
    const float cy = CY + wander * H * 0.5f * sinf(c.s[1] * 0.29f + 1.3f);
    const float is = 1.f / spacing, ph = c.s[0] + c.p[7] * is;  // their motion plus Offset
    const Bands base(soft, step, gap);  // where the softness is wider than the row's footprint
    for (int b = 0; b < NY; b++) {
        const float dy = Y[b] - cy;
        // The ring crosses the rows at a slant |dy| / r: its edge spans FY * |dy| / r across
        const float fy = FY[b] * fabsf(dy) * is;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - cx;
            const float r = sqrtf(dx * dx + dy * dy);
            const float u = r * is - ph;
            if (r > 1e-3f ? fy <= soft * r : FY[b] * is <= soft) o[i] = base.at(u, c.lutWrap);
            else o[i] = Bands(r > 1e-3f ? fy / r : FY[b] * is, step, gap).at(u, c.lutWrap);
        }
    }
}

// ---------------------------------------------------------------- Drops
// Big rings expanding from random points (or from the point Drop X, Y sets) and fading out, like
// raindrops on a pond.

struct Drop { float x, y, r, fade; uint8_t col; bool live; };
static Drop drops[10];

static void dropsFx(Ctx& c) {
    const float rate = c.p[0], speed = c.p[1], width = c.p[2], maxR = c.p[3], bg = c.p[4];
    // Where drops land: aim 0 = anywhere (as before these sliders), 1 = at Drop X, Y
    const float aimX = c.p[5] * W, aimY = Y[0] + c.p[6] * H, aim = c.p[7];
    if (c.s[7] == 0) {
        c.s[7] = 1;
        for (Drop& d : drops) d.live = false;
        c.s[0] = 0.99f;  // first drop right away
    }
    int spawn = 0;
    if (c.cycleBeats > 0) {
        // A drop every N beats, on the beat
        const double every = c.cycleBeats;
        const float idx = (float)floor(c.beat / every);
        if (c.s[6] != (float)every) {
            c.s[6] = (float)every;
            c.s[5] = idx;
        } else if (idx != c.s[5]) {
            c.s[5] = idx;
            spawn = 1;
        }
    } else {
        c.s[6] = 0;
        c.s[0] += c.dt * rate / 60.f;
        while (c.s[0] >= 1.f) {
            c.s[0] -= 1.f;
            spawn++;
        }
    }
    for (; spawn > 0; spawn--) {
        for (Drop& d : drops) {
            if (d.live) continue;
            d = {frand() * W, Y[0] + frand() * H, 0, 0, random8(), true};
            d.x += aim * (aimX - d.x);
            d.y += aim * (aimY - d.y);
            break;
        }
    }
    for (Drop& d : drops) {
        if (!d.live) continue;
        d.r += speed * c.dt;
        if (d.r > maxR) d.live = false;
        d.fade = (1.f - d.r / maxR) * fminf(1.f, d.r / width);  // once per drop, not per LED
    }
    const CRGB back = c.lutLin[0];
    const float sig = width * 0.5f;
    // A row's light, added up drop by drop in the same order as before (so the same sums), but
    // each drop only visits the LEDs of the row its ring can reach
    static float* acc = nullptr;  // r, g, b per LED of a row (heap: static DRAM is full)
    if (!acc) acc = (float*)malloc(3 * MAX_X * sizeof(float));
    static float* ring = nullptr;  // one drop's light along a row, at every other LED
    if (!ring) ring = (float*)malloc((MAX_X + 2) * sizeof(float));
    if (!acc || !ring) return;
    const float pitch = PITCH;
    const float invSig = 1.f / sig, invSig2 = invSig * invSig;
    // Per drop: its colour times fade and ring width, once; a ring under half a colour step is
    // left out (dk[n][3] = 0)
    float dk[10][4];
    for (int n = 0; n < 10; n++) {
        const Drop& d = drops[n];
        const CRGB& k = c.lutWrap[d.col];
        const float f = d.fade * sig;
        dk[n][0] = k.r * f;
        dk[n][1] = k.g * f;
        dk[n][2] = k.b * f;
        dk[n][3] = fmaxf(fmaxf(k.r, k.g), k.b) * d.fade >= 0.5f ? 1.f : 0.f;
    }
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            acc[3 * i] = back.r * bg;
            acc[3 * i + 1] = back.g * bg;
            acc[3 * i + 2] = back.b * bg;
        }
        // Per row: the footprint widening (small: at most ~9% of the ring width), and how far out
        // the band reaches (3.5 of its widest sd: beyond that under half a colour step)
        const float fwk = FY[b] * 0.29f, umax = fwk * fwk * invSig2;
        const float lim = 3.5f * sig * sqrtf(1.f + umax);
        for (int n = 0; n < 10; n++) {
            const Drop& d = drops[n];
            if (!d.live || dk[n][3] == 0) continue;
            const float dy = Y[b] - d.y;
            const float outer = d.r + lim + 0.01f, inner = d.r - lim - 0.01f;
            const float reach2 = outer * outer - dy * dy;
            if (reach2 < 0) continue;  // the ring's band misses this row
            // LEDs within the band's reach along the row, one extra each side for rounding
            const float reach = sqrtf(reach2);
            const int i0 = max(0, (int)floorf((d.x - reach) / pitch - 0.5f) - 1);
            const int i1 = min(NX - 1, (int)ceilf((d.x + reach) / pitch - 0.5f) + 1);
            const float a = umax * dy * dy;  // u = a / dist^2: the widening along the ring's normal
            const float kr = dk[n][0], kg = dk[n][1], kb = dk[n][2];
            // The ring is soft (its width spans ~18 LEDs): computed at every other LED, the ones
            // between take the mean of their neighbours (under 0.2% off). Halves the work.
            const int e0 = i0 & ~1, e1 = min(NX - 1 + ((NX - 1) & 1), (i1 + 1) | 1) & ~1;
            for (int i = e0; i <= e1; i += 2) {
                float I = 0;
                if (i < NX) {
                    const float dx = X[i] - d.x, d2 = dx * dx + dy * dy;
                    if (!(d2 > outer * outer || (inner > 0 && d2 < inner * inner))) {
                        // No square root, divide or expf per LED: 1/sqrt, a series, the exp table
                        const float rd = d2 > 1e-12f ? rsqrtFast(d2) : 0.f;
                        const float off = d2 * rd - d.r;
                        if (fabsf(off) <= lim) {
                            const float u = fminf(a * rd * rd, umax);
                            const float rse = invSig * (1.f - 0.5f * u + 0.375f * u * u);  // 1/se
                            I = expNeg(-0.5f * off * off * rse * rse) * rse;
                        }
                    }
                }
                ring[i] = I;
            }
            for (int i = i0; i <= i1; i++) {
                const float I = (i & 1) ? (i + 1 < NX ? 0.5f * (ring[i - 1] + ring[i + 1]) : ring[i - 1]) : ring[i];
                if (I <= 0) continue;
                acc[3 * i] += kr * I;
                acc[3 * i + 1] += kg * I;
                acc[3 * i + 2] += kb * I;
            }
        }
        for (int i = 0; i < NX; i++)
            o[i] = CRGB((uint8_t)fminf(acc[3 * i], 255.f), (uint8_t)fminf(acc[3 * i + 1], 255.f),
                        (uint8_t)fminf(acc[3 * i + 2], 255.f));
    }
}

// ---------------------------------------------------------------- Interference
// Two sets of concentric waves around centres that slowly circle each other.

static void interference(Ctx& c) {
    const float lambda = c.p[0], speed = c.p[1], sep = c.p[2], rot = c.p[3], contrast = c.p[4],
                drift = c.p[5];
    advance(c, c.s[0], speed / lambda, 1.f);
    c.s[1] += c.dt * rot * DEG;
    c.s[2] += c.dt * drift * 0.02f;
    const float half = sep * 0.5f * fmaxf(W, H) * 0.5f;
    const float x1 = CX + half * cosf(c.s[1]), y1 = CY + half * sinf(c.s[1]);
    const float x2 = CX - half * cosf(c.s[1]), y2 = CY - half * sinf(c.s[1]);
    const float il = 1.f / lambda, ph = c.s[0] + c.p[6] * il;  // their motion plus Offset
    for (int b = 0; b < NY; b++) {
        const float dy1 = Y[b] - y1, dy2 = Y[b] - y2;
        // footAtten as exp(kb * sin^2), sin^2 = dy^2 / r^2 (1 at the centre)
        const float kb = -1.645f * FY[b] * FY[b] * il * il;
        const float q1 = dy1 * dy1, q2 = dy2 * dy2;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float dx1 = X[i] - x1, dx2 = X[i] - x2;
            const float s1 = dx1 * dx1 + q1, s2 = dx2 * dx2 + q2;
            const float r1 = sqrtf(s1), r2 = sqrtf(s2);
            const float v1 = cosCyc(r1 * il - ph) * expNeg(kb * (s1 > 1e-6f ? q1 / s1 : 1.f));
            const float v2 = cosCyc(r2 * il - ph) * expNeg(kb * (s2 > 1e-6f ? q2 / s2 : 1.f));
            const float v = (v1 + v2) * 0.25f + 0.5f;  // 0..1
            o[i] = scaleF(lutW(c.lutWrap, c.s[2] + v * 0.5f), 1.f - contrast * (1.f - v));
        }
    }
}

// ---------------------------------------------------------------- Clouds
// Soft cloud shapes drifting on a wind that slowly turns; coverage from clear to overcast.
// Palette as a ramp: first colour = sky, last = cloud.

// wrap256(v) * 65536 as the noise takes it. v * 65536 is exact (a power of two), so its floor
// modulo 256 cells is the same number as before, without fmodf.
static inline uint32_t cell16(float v) { return (uint32_t)floorI(v * 65536.f) & 0xFFFFFFu; }

static void clouds(Ctx& c) {
    const float cover = c.p[0], size = c.p[1], wind = c.p[2], turn = c.p[3], evolve = c.p[4],
                soft = c.p[5];
    c.s[2] += c.dt * turn * DEG;
    c.s[0] += cosf(c.s[2]) * wind * c.dt;
    c.s[1] += sinf(c.s[2]) * wind * c.dt;
    c.s[3] += c.dt * evolve * 0.05f;
    // Keep the offsets bounded (the noise repeats every 256 cells of the coarsest octave)
    const float period = 256.f * size;
    c.s[0] = fmodf(c.s[0], period);
    c.s[1] = fmodf(c.s[1], period);
    const float thr = 1.f - 2.f * cover, sw = 0.03f + soft * 0.5f;
    const float e0 = thr - sw, ie = 1.f / (2.f * sw);
    const float sx = c.s[0] + c.p[6], sy = c.s[1] + c.p[7];  // the wind's drift plus Offset
    // fbm, as fbm() does it: octaves finer than 0.8 m are left out. What doesn't change along a
    // row is worked out once per frame (time) or per row (y).
    int oct = 0;
    float lam[4], wt[4], tw = 0;
    uint32_t tz[4];
    for (float l = size, w = 1; oct < 4 && (oct == 0 || l >= 0.8f); oct++, l *= 0.5f, w *= 0.5f) {
        lam[oct] = l;
        wt[oct] = w;
        tw += w;
        tz[oct] = (uint32_t)(wrap256(c.s[3] * (1 + oct * 0.7f) + oct * 47.1f) * 65536.f);
    }
    // Clouds are smooth along a row: sample every ~8 cm and interpolate
    const float span = NX == NXFULL ? W : NX * PITCH;  // the width rendered (in mirror, half)
    const int ns = clampi((int)(span / 0.08f) + 2, 2, NX);
    static float v[MAX_X];
    const float istep = NX > 1 ? (float)(ns - 1) / (NX - 1) : 0;
    for (int b = 0; b < NY; b++) {
        uint32_t ty[4];
        for (int o = 0; o < oct; o++) ty[o] = cell16((Y[b] - sy) / lam[o] + o * 31.7f);
        for (int k = 0; k < ns; k++) {
            const float x = X[0] + (X[NX - 1] - X[0]) * k / (ns - 1) - sx;
            float sum = 0;
            for (int o = 0; o < oct; o++)
                sum += ((int)inoise16(cell16(x / lam[o] + o * 17.3f), ty[o], tz[o]) - 32768) * wt[o];
            v[k] = sum / tw * noiseScale;
        }
        CRGB* out = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float fk = i * istep;
            int k0 = (int)fk;
            if (k0 >= ns - 1) k0 = ns - 2;
            const float val = v[k0] + (v[k0 + 1] - v[k0]) * (fk - k0);
            out[i] = lutL(c.lutLin, sm((val - e0) * ie));
        }
    }
}

// ---------------------------------------------------------------- Sparkle
// Twinkles in palette colours over a dim palette wash.

static void sparkle(Ctx& c) {
    const float density = c.p[0], life = c.p[1], bg = c.p[2], drift = c.p[3];
    c.s[0] += c.dt * drift * 0.02f;
    sparkleStep(density, life, c.dt);
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const int idx = b * NX + i;
            const CRGB base = scaleF(lutW(c.lutWrap, X[i] / W * 0.5f + (Y[b] - Y[0]) / H * 0.3f + c.s[0]), bg);
            if (!spPhase[idx]) { o[i] = base; continue; }
            o[i] = mixF(base, c.lutWrap[spColour[idx]], sparkleLevel(spPhase[idx]));
        }
    }
}

// ---------------------------------------------------------------- Gradient
// A steady gradient through the palette: straight in any direction, or round from a centre. The
// palette is used as a ramp there and back (first -> last -> first), so a drift or a wide spread
// never jumps from the last colour to the first.

static void gradient(Ctx& c) {
    const float ang = c.p[0] * DEG, spread = c.p[1], offset = c.p[2], drift = c.p[3];
    const bool radial = c.p[4] >= 0.5f;
    const float cx = c.p[5] * W, cy = Y[0] + c.p[6] * H;
    c.s[0] += c.dt * drift * 0.02f;
    c.s[0] -= floorf(c.s[0] * 0.5f) * 2.f;  // bounded: the ramp repeats every 2
    const float base = offset + c.s[0];
    float k, k0;  // t = k * (distance along the gradient) + k0
    const float ca = cosf(ang), sa = sinf(ang);
    if (radial) {
        float maxR = 0;
        for (float x : {0.f, W})
            for (float y : {Y[0], Y[0] + H}) maxR = fmaxf(maxR, hypotf(x - cx, y - cy));
        k = spread / fmaxf(maxR, 0.01f);
        k0 = base;
    } else {
        // The ceiling's extent in that direction: the gradient runs from one side to the other
        const float span = fmaxf(fabsf(W * ca) + fabsf(H * sa), 0.01f);
        k = spread / span;
        k0 = base + 0.5f * spread;  // centred on the ceiling's middle
    }
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        const float dy = Y[b] - (radial ? cy : CY);
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - (radial ? cx : CX);
            const float d = radial ? sqrtf(dx * dx + dy * dy) : dx * ca + dy * sa;
            float t = d * k + k0;
            t -= floorI(t * 0.5f) * 2.f;  // 0..2: there and back along the palette
            o[i] = lutL(c.lutLin, t <= 1.f ? t : 2.f - t);
        }
    }
}

// ---------------------------------------------------------------- Shapes
// Squares, circles and triangles appearing one after another at random places, sizes, angles and
// palette colours; each stays for Count turns and fades out. Outline, solid, or a dim fill inside a
// bright outline. Shape n's looks come from a hash of n, so it needs no state of its own.

static inline float hrand(uint32_t n, uint32_t k) {
    uint32_t x = n * 2654435761u + k * 40503u + 977u;
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return (x >> 8) / 16777216.f;
}

static void shapesFx(Ctx& c) {
    const int kind = clampi((int)(c.p[0] + 0.5f), 0, 3), count = clampi((int)(c.p[5] + 0.5f), 1, 6);
    const float size = c.p[1], variety = c.p[2], fill = c.p[3], half = c.p[4] * 0.5f, every = c.p[6],
                spin = c.p[7] * DEG;
    advance(c, c.s[0], 1.f / every, 1.f);  // one new shape per cycle (in beats: on the beat)
    const float clock = c.s[0];
    const int J = floorI(clock);
    // How long a shape lives, in seconds (for its spin)
    const float lifeS = (c.cycleBeats > 0 ? c.cycleBeats * 60.f / fmaxf(c.bpm, 1.f) : every) * count;
    struct Sh { float x, y, r, env, reach; float nx[4], ny[4]; uint8_t type, faces; CRGB col; } sh[6];
    int n = 0;
    for (int k = 0; k < count; k++) {
        const int idx = J - k;
        const float f = (clock - idx) / count;  // 0..1 through its life
        if (f < 0 || f >= 1) continue;
        const uint32_t u = (uint32_t)idx;
        Sh& s = sh[n];
        s.type = kind < 3 ? kind : (uint8_t)min(2, (int)(hrand(u, 1) * 3));
        const float w = size * (1.f - variety * hrand(u, 2));  // its width in metres
        const float a = hrand(u, 5) * TAU + (hrand(u, 6) < 0.5f ? -spin : spin) * f * lifeS;
        s.env = sm(f * 12.5f) * sm((1.f - f) * 3.33f);  // quick fade in, slower fade out
        s.col = c.lutWrap[(uint8_t)(hrand(u, 7) * 256.f)];
        if (s.type == 0) {  // square: four faces, half a side from the centre
            s.faces = 4;
            s.r = w * 0.5f;
            for (int q = 0; q < 4; q++) { s.nx[q] = cosf(a + q * 1.5707963f); s.ny[q] = sinf(a + q * 1.5707963f); }
            s.reach = s.r * 1.4143f;
        } else if (s.type == 2) {  // triangle, w = its side: three faces at the inradius
            s.faces = 3;
            s.r = w * 0.288675f;
            for (int q = 0; q < 3; q++) { s.nx[q] = cosf(a + q * 2.0943951f); s.ny[q] = sinf(a + q * 2.0943951f); }
            s.reach = s.r * 2.f;
        } else {  // circle, w = its diameter
            s.faces = 0;
            s.r = w * 0.5f;
            s.reach = s.r;
        }
        // Its centre: anywhere it fits inside the ceiling whole (a shape too big for that: centred)
        const float mx = fminf(s.reach + half, W * 0.5f), my = fminf(s.reach + half, H * 0.5f);
        s.x = mx + hrand(u, 3) * (W - 2 * mx);
        s.y = Y[0] + my + hrand(u, 4) * (H - 2 * my);
        s.reach += half + 0.6f;  // the line and the soft edge
        n++;
    }
    memset(c.out, 0, NL * sizeof(CRGB));
    const float pitch = PITCH, ip = 1.f / pitch;
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        for (int j = 0; j < n; j++) {
            const Sh& s = sh[j];
            if (s.env <= 0.002f) continue;
            const float dy = Y[b] - s.y;
            if (fabsf(dy) >= s.reach) continue;
            const float rx = sqrtf(s.reach * s.reach - dy * dy);
            const int i0 = max(0, floorI((s.x - rx) * ip)), i1 = min(NX - 1, (int)((s.x + rx) * ip) + 1);
            // An edge's softness: a little along the rows, the row's footprint where it crosses
            // them (a face's slant to the rows), so it doesn't pop from one row to the next.
            // Per face once a row: 1 / width
            float ie[4];
            for (int q = 0; q < s.faces; q++) ie[q] = 1.f / (0.12f + FY[b] * fabsf(s.ny[q]));
            for (int i = i0; i <= i1; i++) {
                const float dx = X[i] - s.x;
                float d, inv;
                if (s.faces) {  // distance outside the nearest face (inside: negative)
                    int best = 0;
                    float m = dx * s.nx[0] + dy * s.ny[0];
                    for (int q = 1; q < s.faces; q++) {
                        const float v = dx * s.nx[q] + dy * s.ny[q];
                        if (v > m) { m = v; best = q; }
                    }
                    d = m - s.r;
                    inv = ie[best];
                } else {
                    const float d2 = dx * dx + dy * dy;
                    const float rd = d2 > 1e-8f ? rsqrtFast(d2) : 0.f;
                    d = d2 * rd - s.r;
                    const float e = 0.12f + FY[b] * fabsf(dy) * rd;
                    inv = rsqrtFast(e * e);
                }
                // Coverage of the outline band and of the inside, over the edge's width: a line
                // narrower than a row's footprint comes out dimmer instead of popping
                const float line = sm(0.5f - (fabsf(d) - half) * inv);
                const float inside = fill * sm(0.5f - d * inv);
                const float I = fmaxf(line, inside) * s.env;
                if (I < 0.002f) continue;
                const CRGB v = scaleF(s.col, I);
                CRGB& t = o[i];
                if (v.r > t.r) t.r = v.r;
                if (v.g > t.g) t.g = v.g;
                if (v.b > t.b) t.b = v.b;
            }
        }
    }
}

// ---------------------------------------------------------------- table
// New effects go at the end: presets store the index.

// Shared help texts
#define H_BG "How bright the background is where nothing else shines, in the palette's first colour. 0 = black."
#define H_DRIFT "How fast the colours slowly move through the palette. 0 = they stay put."
#define H_GAPS "How dark the borders between neighbouring bands get. 0 = the colours touch, 1 = black between them."
#define H_STEP "How far along the palette each next band moves. Small = neighbours in similar colours, 0.5 = opposite colours in turn, 0 or 1 = every band the same colour."
#define H_SOFT_BANDS "How gradually one band blends into the next. 0 = crisp edges (still softened where they cross the bars, so they don't flicker), 1 = smooth gradients."

const EffectDef EFFECTS[] = {
    {"Rolling bars", 8,
     {{"Speed (m/s)", 0, 8, 0.15f, 0.01f, "How fast the bands roll, in metres per second. 0 stops them; the direction still turns. Calm: 0.05 to 0.2."},
      {"Band width (m)", 0.8f, 5, 2, 0.1f, "The width of one band, in metres. Narrow bands flicker as they cross the rows: with rows half a metre apart, for example, about 0.8 m is the narrowest that stays smooth."},
      {"Softness", 0, 1, 0.6f, 0.01f, H_SOFT_BANDS},
      {"Dark gaps", 0, 1, 0.3f, 0.01f, H_GAPS},
      {"Angle (deg)", -180, 180, 0, 1, "The direction the bands roll, in degrees: 0 = along the rows, 90 = across them (from the first row to the last), 180 = along the rows the other way. A Motion on it swings the direction to and fro."},
      {"Colour step", 0, 1, 0.2f, 0.01f, H_STEP},
      {"Offset (m)", -10, 10, 0, 0.01f, "Shifts the pattern by this many metres, the way Speed moves the bands (in the rolling direction). With Speed at 0 it stays put, so a Motion on Offset moves it back and forth (with easing). 0 = no shift."},
      {"Gap width", 0, 0.9f, 0, 0.01f, "How wide the dark gaps between the bands are, as a share of a band: 0 = only where the colours meet, 0.5 = half dark, 0.9 = thin bars of colour with wide gaps. Dark gaps sets how dark they get (at Dark gaps 0 they don't show)."}},
     rolling, false, "band", 0,
     "Wide soft bands of palette colour rolling across the ceiling, in the direction Angle sets. In Beats timing, each band takes the chosen number of beats to pass."},
    {"Searchlights", 5,
     {{"Beams", 1, 5, 3, 1, "How many round beams wander over the ceiling, each in its own palette colour. Where beams overlap their light adds up."},
      {"Beam radius (m)", 0.5f, 3, 1.2f, 0.1f, "The radius of each beam in metres: 1.2 m makes a beam about 2.4 m across."},
      {"Speed", 0, 1.5f, 0.3f, 0.01f, "How fast the beams wander along their looping paths. 0.3 is a slow drift, 1.5 a brisk sweep, 0 freezes them."},
      {"Background", 0, 0.3f, 0.02f, 0.01f, H_BG},
      {"Softness", 0, 1, 0.6f, 0.01f, "How far in from the edge a beam fades. 0 = an evenly lit disc with a short soft edge (never under 0.4 m), 1 = brightest only at the centre."}},
     searchlights, false, nullptr, -1,
     "Soft round beams wandering over the ceiling on slow looping paths, like searchlights in the sky. Their paths don't repeat for a long time."},
    {"Lighthouse", 8,
     {{"Speed (rpm)", -20, 20, 4, 0.1f, "Turns per minute; negative turns the other way. In Beats timing this slider is replaced by beats per revolution."},
      {"Beam width (deg)", 5, 120, 30, 1, "How wide the beam is, as an angle. The beam gets wider in metres the further it is from the centre."},
      {"Beams", 1, 4, 1, 1, "How many beams, evenly spaced around the centre like a lighthouse with several lamps. Each spot is passed this many times per turn."},
      {"Origin X", -0.2f, 1.2f, 0.5f, 0.01f, "Where the centre sits along the bars: 0 = one end, 0.5 = the middle, 1 = the other end. Below 0 or above 1 puts it outside the ceiling."},
      {"Origin Y", -0.2f, 1.2f, -0.05f, 0.01f, "Where the centre sits across the bars: 0 = the first bar, 1 = the last bar. The default, -0.05, is just outside the first bar, so the beam sweeps in from the edge."},
      {"Background", 0, 0.3f, 0.02f, 0.01f, H_BG},
      {"Colour spread", 0, 1, 0.6f, 0.01f, "How the beam's colour changes with distance from the centre. 0 = all in the palette's first colour, 1 = through the whole palette out to the farthest corner."},
      {"Angle (deg)", -180, 180, 0, 1, "Which way the beam points, in degrees: 0 = along the bars (towards their far end), 90 = across them (towards the last bar), -90 = towards the first bar, 180 = back along the bars. With Speed at 0 the beam stays there; while it turns, this sets where the turn starts, so it can also line a turn up with the beat. With several beams it is the first one's direction."}},
     lighthouse, false, "revolution", 0,
     "A beam turning around a point, like a lighthouse. The point can sit inside the ceiling, at its edge or beyond it. In Beats timing, one revolution takes the chosen number of beats."},
    {"Clouds", 8,
     {{"Coverage", 0, 1, 0.5f, 0.01f, "How much of the sky is cloud: 0 = clear, 0.5 = half cloudy, 1 = overcast."},
      {"Cloud size (m)", 1, 8, 3, 0.1f, "The typical size of a cloud in metres. Smaller clouds still keep their detail coarse enough for the bar spacing."},
      {"Wind (m/s)", 0, 1, 0.15f, 0.01f, "How fast the clouds drift, in metres per second. 0 = they stay in place (they can still evolve)."},
      {"Wind turn (deg/s)", -5, 5, 0.5f, 0.1f, "How fast the wind direction turns, in degrees per second; negative turns the other way. 0 = a steady wind."},
      {"Evolve", 0, 1, 0.2f, 0.01f, "How fast the cloud shapes change as they drift. 0 = rigid shapes sliding past, 1 = clouds forming and dissolving quickly."},
      {"Softness", 0, 1, 0.4f, 0.01f, "How gradually a cloud fades into the sky. 0 = crisp edges, 1 = hazy."},
      {"Offset X (m)", -20, 20, 0, 0.01f, "Shifts the clouds along the bars by this many metres, on top of the wind. With Wind at 0 they stay put, so a Motion on Offset X moves them back and forth. 0 = no shift."},
      {"Offset Y (m)", -20, 20, 0, 0.01f, "Shifts the clouds across the bars by this many metres, on top of the wind. With Wind at 0 they stay put, so a Motion on Offset Y moves them back and forth. 0 = no shift."}},
     clouds, false, nullptr, -1,
     "Soft clouds drifting on a slowly turning wind. The palette is used as a ramp: its first colour is the sky, its last colour the clouds."},
    {"Sparkle", 4,
     {{"Density", 0, 1, 0.4f, 0.01f, "How many LEDs twinkle at once. 1 = about 8% of the LEDs lit at any moment."},
      {"Twinkle time (s)", 0.3f, 5, 1.5f, 0.1f, "How long one twinkle lasts, in seconds: a quick rise and a slow fade."},
      {"Background", 0, 0.5f, 0.05f, 0.01f, "How bright the slowly drifting palette wash under the twinkles is. 0 = black."},
      {"Drift", 0, 1, 0.2f, 0.01f, "How fast the background wash moves through the palette."}},
     sparkle, true, nullptr, -1,
     "Single LEDs twinkling in palette colours over a dim palette wash. For twinkles on top of another effect, use the Sparkle overlay (under Brightness) instead."},
    {"Sweeps", 8,
     {{"Bars", 1, 4, 1, 1, "How many bars travel at once, evenly spaced. Each changes direction on its own."},
      {"Speed (m/s)", 0.05f, 2, 0.4f, 0.01f, "How fast a bar crosses the ceiling, in metres per second. In Beats timing this slider is replaced by beats per sweep."},
      {"Bar width (m)", 0.3f, 4, 1, 0.1f, "The width of the solid part of each bar, in metres."},
      {"Trail (m)", 0, 6, 1.5f, 0.1f, "How far behind the bar its glow fades out, in metres. 0 = no trail."},
      {"Angle change (deg)", 0, 180, 37, 1, "How much the direction turns after each pass. 180 = back and forth, 90 = crosswise in turn, 0 = always the same way. Odd angles such as 37 rarely repeat."},
      {"Angle (deg)", -180, 180, 0, 1, "Turns the direction of every bar by this many degrees: with Angle change at 0 and one bar, the direction it sweeps (0 = along the rows, 90 = across them). Each bar takes it as its pass begins and keeps it to the end, so a controller or Motion on it (a Random jump, say) sets the direction of the next pass, never turns a bar halfway."},
      {"Background", 0, 0.3f, 0.02f, 0.01f, H_BG},
      {"Softness", 0, 1, 0.2f, 0.01f, "How soft the bar's edges are, as a part of its width. 0 = crisp (still softened where they cross the bars)."}},
     sweeps, false, "sweep", 1,
     "Bars sweeping across the ceiling one after another, each pass at a new angle, with a fading trail. Each new pass takes the next palette colour."},
    {"Plaid", 7,
     {{"Stripe period (m)", 1, 6, 2.5f, 0.1f, "The distance from one stripe to the next, in metres."},
      {"Speed (m/s)", 0, 1, 0.1f, 0.01f, "How fast the stripes move, in metres per second; the two sets move opposite ways. In Beats timing this slider is replaced by beats per stripe."},
      {"Turn (deg/s)", -10, 10, 1, 0.1f, "How fast the whole pattern turns, in degrees per second; negative turns the other way."},
      {"Cross angle (deg)", 0, 90, 60, 1, "The angle between the two stripe sets. 90 = square checks, smaller = stretched diamonds, 0 = both sets parallel."},
      {"Contrast", 0, 1, 0.6f, 0.01f, "How dark it gets between the stripes. 0 = even brightness, only the colours change, 1 = deep dark gaps."},
      {"Colour drift", 0, 1, 0.2f, 0.01f, H_DRIFT},
      {"Offset (m)", -10, 10, 0, 0.01f, "Shifts the pattern by this many metres, the way Speed moves the stripes (the second set goes the other way). With Speed at 0 it stays put, so a Motion on Offset moves it back and forth (with easing). 0 = no shift."}},
     plaid, false, "stripe", 1,
     "Two sets of broad stripes crossing at an angle, moving and slowly turning. The colour shows which of the two sets is stronger at each spot."},
    {"Rings", 8,
     {{"Ring spacing (m)", 0.8f, 4, 1.5f, 0.1f, "The distance from one ring to the next, in metres."},
      {"Speed (m/s, - = inward)", -1, 1, 0.2f, 0.01f, "How fast the rings flow, in metres per second: positive outward from the centre, negative inward. In Beats timing this slider is replaced by beats per ring."},
      {"Softness", 0, 1, 0.6f, 0.01f, H_SOFT_BANDS},
      {"Dark gaps", 0, 1, 0.4f, 0.01f, H_GAPS},
      {"Centre wander", 0, 1, 0.4f, 0.01f, "How far the centre roams. 0 = fixed in the middle of the ceiling, 1 = all the way to the edges."},
      {"Wander speed", 0, 1, 0.2f, 0.01f, "How fast the centre roams."},
      {"Colour step", 0, 1, 0.2f, 0.01f, H_STEP},
      {"Offset (m)", -10, 10, 0, 0.01f, "Shifts the pattern by this many metres, the way Speed moves the rings (positive outward). With Speed at 0 it stays put, so a Motion on Offset moves it back and forth (with easing). 0 = no shift."}},
     rings, false, "ring", 1,
     "Concentric rings flowing out of (or into) a centre that slowly wanders over the ceiling."},
    {"Drops", 8,
     {{"Drops per minute", 1, 60, 10, 1, "How many new drops fall per minute. At most 10 rings show at once. In Beats timing this slider is replaced by beats per drop, and each drop lands on the beat."},
      {"Speed (m/s)", 0.1f, 4, 0.5f, 0.01f, "How fast a ring spreads, in metres per second."},
      {"Ring width (m)", 0.4f, 2.5f, 1, 0.1f, "The width of a ring's bright band, in metres."},
      {"Max radius (m)", 1, 12, 6, 0.1f, "How far a ring spreads before it has faded out completely, in metres."},
      {"Background", 0, 0.3f, 0.03f, 0.01f, H_BG},
      {"Drop X", 0, 1, 0.5f, 0.01f, "Where along the rows the drops land: 0 = the start of the rows, 1 = their end. Only used when Aim is above 0: at Aim 0 drops fall at random places and Drop X, Y do nothing. A Motion on it moves the landing point; each ring stays where its drop fell."},
      {"Drop Y", 0, 1, 0.5f, 0.01f, "Where across the rows the drops land: 0 = the first row, 1 = the last. Only used when Aim is above 0: at Aim 0 drops fall at random places and Drop X, Y do nothing. A Motion on it moves the landing point; each ring stays where its drop fell."},
      {"Aim (0 = random)", 0, 1, 1, 0.01f, "Where drops land. 0 = at random places anywhere on the ceiling (Drop X, Y do nothing). 1 = exactly at Drop X, Y. In between: scattered around Drop X, Y, the closer to 1 the tighter."}},
     dropsFx, false, "drop", 0,
     "Big rings spreading from where drops land and fading out, like raindrops on a pond: at the point Drop X, Y sets (which Motions can move), or at random places. Each drop takes a random palette colour."},
    {"Interference", 7,
     {{"Wavelength (m)", 0.8f, 5, 1.8f, 0.1f, "The distance from one wave to the next, in metres."},
      {"Speed (m/s)", -1, 1, 0.15f, 0.01f, "How fast the waves travel, in metres per second: positive outward, negative inward. In Beats timing this slider is replaced by beats per wave."},
      {"Centre distance", 0, 1, 0.5f, 0.01f, "How far apart the two wave centres are. 0 = the same point (plain rings), 1 = about the ceiling's size apart."},
      {"Rotate (deg/s)", -10, 10, 2, 0.1f, "How fast the two centres circle each other, in degrees per second; negative the other way."},
      {"Contrast", 0, 1, 0.7f, 0.01f, "How dark the troughs get where the waves cancel. 0 = even brightness, 1 = black."},
      {"Colour drift", 0, 1, 0.2f, 0.01f, H_DRIFT},
      {"Offset (m)", -10, 10, 0, 0.01f, "Shifts the pattern by this many metres, the way Speed moves the waves (positive outward). With Speed at 0 it stays put, so a Motion on Offset moves it back and forth (with easing). 0 = no shift."}},
     interference, false, "wave", 1,
     "Two sets of ring waves around centres that slowly circle each other. Where the waves meet they reinforce or cancel, forming large moving patterns."},
    {"Gradient", 7,
     {{"Angle (deg)", -180, 180, 0, 1, "The direction of a straight gradient, in degrees: 0 = along the bars, 90 = across them (from the first bar to the last), 45 = diagonally. Not used when Radial is 1."},
      {"Spread", 0.1f, 4, 1, 0.01f, "How much of the palette spans the ceiling. 1 = from the palette's first colour at one side to its last colour at the other; 2 = there and back; 0.5 = only the first half of the palette. Bigger means more colour changes across the ceiling."},
      {"Offset", -2, 2, 0, 0.01f, "Shifts the colours along the gradient: 1 moves them by a whole palette (first colour to last). A Motion on Offset makes the colours flow back and forth."},
      {"Drift", 0, 1, 0, 0.01f, "Lets the colours flow slowly along the gradient by themselves. 0 = a steady gradient that never changes."},
      {"Radial", 0, 1, 0, 1, "0 = a straight gradient in the direction of Angle; 1 = a round gradient, from the centre (Centre X, Centre Y) outwards."},
      {"Centre X", -0.2f, 1.2f, 0.5f, 0.01f, "Radial: where the centre sits along the bars: 0 = one end, 0.5 = the middle, 1 = the other end. Beyond 0 or 1 puts it outside the ceiling."},
      {"Centre Y", -0.2f, 1.2f, 0.5f, 0.01f, "Radial: where the centre sits across the bars: 0 = the first bar, 0.5 = the middle, 1 = the last bar."}},
     gradient, false, nullptr, -1,
     "A steady gradient through the palette: straight in any direction, or round from a centre. The palette is used as a ramp, first colour to last and back. Calm light that doesn't move unless Drift or a Motion moves it."},
    {"Shapes", 8,
     {{"Shape", 0, 3, 3, 1, "0 = squares, 1 = circles, 2 = triangles, 3 = a mix of all three, picked at random."},
      {"Size (m)", 0.8f, 8, 3, 0.1f, "How big the shapes are, in metres: a circle's diameter, a square's or a triangle's side. Below about 1.5 m they get hard to read across rows half a metre apart, for example."},
      {"Size variety", 0, 1, 0.5f, 0.01f, "How much the sizes differ. 0 = all the same size, 1 = anything from Size down to tiny."},
      {"Fill", 0, 1, 0, 0.01f, "0 = outlines only, 1 = solid shapes. In between: a dimmer inside within a bright outline."},
      {"Line width (m)", 0.2f, 2, 0.6f, 0.05f, "How thick the outlines are, in metres. Lines thinner than the row spacing come out dimmer where they run along the rows, so they don't flicker."},
      {"Count", 1, 6, 3, 1, "How many shapes show at once. Each stays for this many turns and fades out as a new one appears."},
      {"New shape every (s)", 0.2f, 10, 1.5f, 0.1f, "How often a new shape appears, in seconds. Each lasts Count times this. In Beats timing this slider is replaced by beats per shape, and each lands on the beat."},
      {"Spin (deg/s)", 0, 90, 10, 1, "How fast the shapes turn while they show, each its own way round. 0 = they keep their angle. Circles don't show it."}},
     shapesFx, false, "shape", 6,
     "Squares, circles and triangles appearing one after another at random places, sizes and angles, each in a random palette colour, as outlines, solid or both. Each stays a while and fades out."},
};const int NUM_EFFECTS = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

void effectsInit() {
    fastInit();
    NX = layout.nx;
    NY = layout.ny;
    NL = NX * NY;
    const float pitch = layout.length / NX;
    PITCH = pitch;
    NXFULL = NX;
    for (int i = 0; i < NX; i++) X[i] = (i + 0.5f) * pitch;
    const bool measured = layout.measured && NY == cfg::CEILING_ROWS;
    const float spacing = layout.aspect * pitch;
    for (int b = 0; b < NY; b++) Y[b] = measured ? cfg::CEILING_Y[b] : b * spacing;
    for (int b = 0; b < NY; b++) {
        if (NY == 1) { FY[b] = spacing; break; }
        const float lo = b > 0 ? Y[b - 1] : Y[b] - (Y[b + 1] - Y[b]);
        const float hi = b < NY - 1 ? Y[b + 1] : Y[b] + (Y[b] - Y[b - 1]);
        FY[b] = (hi - lo) * 0.5f;
    }
    W = layout.length;
    H = fmaxf(Y[NY - 1] - Y[0], pitch);
    CX = W * 0.5f;
    CY = Y[0] + (Y[NY - 1] - Y[0]) * 0.5f;
    memset(spPhase, 0, sizeof(spPhase));

    // Scale the noise so a single octave has a standard deviation of ~0.45
    static bool calibrated = false;
    if (calibrated) return;
    calibrated = true;
    double sum = 0, sum2 = 0;
    const int N = 4000;
    for (int i = 0; i < N; i++) {
        const int n = (int)inoise16(i * 40503u, i * 92821u + 12345u, i * 31337u + 777u) - 32768;
        sum += n;
        sum2 += (double)n * n;
    }
    const double mean = sum / N, sd = sqrt(sum2 / N - mean * mean);
    if (sd > 100) noiseScale = (float)(0.45 / sd);
}

// ---------------------------------------------------------------- mirror

void effectsRender(const EffectDef& e, Ctx& c, bool mirror) {
    if (!mirror || NX < 2) {
        e.render(c);
        return;
    }
    // The effect sees a grid of the left half only (rows packed at that width), then each row is
    // spread out to its full width from the last row back, so nothing is overwritten before it is read
    const int full = NX, fullL = NL, half = (NX + 1) / 2;
    NX = half;
    NL = half * NY;
    e.render(c);
    NX = full;
    NL = fullL;
    for (int b = NY - 1; b >= 0; b--) {
        const CRGB* src = c.out + b * half;
        CRGB* dst = c.out + b * full;
        for (int i = half - 1; i >= 0; i--) {
            const CRGB v = src[i];
            dst[full - 1 - i] = v;
            dst[i] = v;
        }
    }
}

// ---------------------------------------------------------------- benchmark
// Time to render effect e with its default parameters, in ms per frame, into a scratch buffer.
// The effects' shared state is saved and put back, so the effect playing carries on as it was.
float effectsBenchmark(int e, const CRGB* lutWrap, const CRGB* lutLin, int frames, bool mirror) {
    if (e < 0 || e >= NUM_EFFECTS || frames < 1) return -1;
    CRGB* out = (CRGB*)malloc(NL * sizeof(CRGB));
    uint16_t* phase = (uint16_t*)malloc(NL * sizeof(uint16_t));
    uint8_t* colour = (uint8_t*)malloc(NL);
    if (!out || !phase || !colour) {
        free(out); free(phase); free(colour);
        return -1;
    }
    memcpy(phase, spPhase, NL * sizeof(uint16_t));
    memcpy(colour, spColour, NL);
    float ang[4], prev[4], at[4];
    uint8_t col[4];
    Drop drp[10];
    memcpy(ang, sweepAng, sizeof(ang)); memcpy(prev, sweepPrev, sizeof(prev)); memcpy(at, sweepAt, sizeof(at));
    memcpy(col, sweepCol, sizeof(col)); memcpy(drp, drops, sizeof(drp));

    const EffectDef& d = EFFECTS[e];
    float p[MAXP] = {}, s[MAXP] = {};
    for (int j = 0; j < d.n; j++) p[j] = d.params[j].def;
    Ctx c{p, 1.f / 40, s, lutWrap, lutLin, out, 0, 120, 0, false};
    effectsRender(d, c, mirror);  // first frame: state set up
    const uint32_t t0 = micros();
    for (int k = 0; k < frames; k++) {
        c.beat += 0.05;
        effectsRender(d, c, mirror);
    }
    const float ms = (micros() - t0) / 1000.f / frames;

    memcpy(spPhase, phase, NL * sizeof(uint16_t));
    memcpy(spColour, colour, NL);
    memcpy(sweepAng, ang, sizeof(ang)); memcpy(sweepPrev, prev, sizeof(prev)); memcpy(sweepAt, at, sizeof(at));
    memcpy(sweepCol, col, sizeof(col)); memcpy(drops, drp, sizeof(drp));
    free(out); free(phase); free(colour);
    return ms;
}
