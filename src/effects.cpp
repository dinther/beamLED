#include "effects.h"

#include <math.h>
#include <string.h>

#include "config.h"

static constexpr float DEG = 0.01745329f;
static constexpr float TAU = 6.2831853f;

static float X[MAX_X];   // m along a row
static float Y[MAX_Y];   // m across the rows
static float FY[MAX_Y];  // footprint of a row across the rows (m)
static int NX = 1, NY = 1, NL = 1;
static float CX, CY, W, H;
static float noiseScale = 1.f / 15000.f;

// ---------------------------------------------------------------- helpers

static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

static inline float sstep(float e0, float e1, float x) {
    float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3 - 2 * t);
}

static inline CRGB lutW(const CRGB* l, float pos) { return l[(int)floorf(pos * 256.f) & 255]; }

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

void sparkleOverlay(CRGB* out, float density, float dt) {
    sparkleStep(density, 1.2f, dt);
    for (int i = 0; i < NL; i++) {
        if (!spPhase[i]) continue;
        uint8_t a = (uint8_t)(sparkleLevel(spPhase[i]) * 255.f);
        out[i] += CRGB(a, a, a);
    }
}

// ---------------------------------------------------------------- Rolling bars
// Wide bands rolling across the grid; their direction slowly turns.

static void rolling(Ctx& c) {
    const float speed = c.p[0], width = c.p[1], soft = c.p[2], gap = c.p[3], turn = c.p[4],
                step = c.p[5];
    c.s[0] += c.dt * turn * DEG;
    advance(c, c.s[1], speed / width, 1.f);  // in beats: one band per cycle
    const float ca = cosf(c.s[0]), sa = sinf(c.s[0]);
    for (int b = 0; b < NY; b++) {
        // Transition at least the row's footprint projected onto the roll direction
        const float k = fmaxf(soft, FY[b] * fabsf(sa) / width);
        const float dy = (Y[b] - CY) * sa;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++)
            o[i] = bands(((X[i] - CX) * ca + dy) / width - c.s[1], k, step, gap, c.lutWrap);
    }
}

// ---------------------------------------------------------------- Sweeps
// Bars sweeping across the grid one after another, each pass at a new angle, with a trail.

static float sweepAng[4], sweepPrev[4];
static uint8_t sweepCol[4];

static void sweeps(Ctx& c) {
    const int n = clampi((int)(c.p[0] + 0.5f), 1, 4);
    const float speed = c.p[1], width = c.p[2], trail = c.p[3], change = c.p[4], rnd = c.p[5],
                bg = c.p[6], soft = c.p[7];
    if (c.s[7] == 0) {
        c.s[7] = 1;
        for (int k = 0; k < 4; k++) {
            sweepAng[k] = k * 1.3f;
            sweepCol[k] = k * 64;
            sweepPrev[k] = 1e9f;
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
            sweepAng[k] = rnd >= 0.5f ? frand() * TAU : sweepAng[k] + change * DEG;
            sweepCol[k] += 51;
        }
        sweepPrev[k] = p;
        pos[k] = p - L * 0.5f;
        ca[k] = cosf(sweepAng[k]);
        sa[k] = sinf(sweepAng[k]);
        col[k] = c.lutWrap[sweepCol[k]];
    }
    const CRGB back = c.lutLin[0];
    const float hw = width * 0.5f;
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        float edge[4];
        for (int k = 0; k < n; k++) edge[k] = fmaxf(fmaxf(soft * width, FY[b] * fabsf(sa[k])), 0.05f);
        for (int i = 0; i < NX; i++) {
            float r = back.r * bg, g = back.g * bg, bl = back.b * bg;
            for (int k = 0; k < n; k++) {
                // s > 0 ahead of the bar's centre, s < 0 behind it
                const float s = (X[i] - CX) * ca[k] + (Y[b] - CY) * sa[k] - pos[k];
                float I;
                if (s > hw) I = 1.f - sstep(hw, hw + edge[k], s);
                else if (s >= -hw) I = 1.f;
                else {
                    I = 1.f - sstep(-hw, -hw - edge[k], s);
                    if (trail > 0.01f) I = fmaxf(I, expf((s + hw) / trail));
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
    const float c1 = cosf(a1), s1 = sinf(a1), c2 = cosf(a2), s2 = sinf(a2);
    for (int b = 0; b < NY; b++) {
        const float at1 = footAtten(b, fabsf(s1), period), at2 = footAtten(b, fabsf(s2), period);
        const float dy = Y[b] - CY;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - CX;
            const float v1 = cosf(TAU * ((dx * c1 + dy * s1) / period - c.s[1])) * at1;
            const float v2 = cosf(TAU * ((dx * c2 + dy * s2) / period + c.s[1] * 0.7f)) * at2;
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
    const float size2 = size * size;
    const CRGB back = c.lutWrap[0];
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            float r = back.r * bg, g = back.g * bg, bl = back.b * bg;
            for (int k = 0; k < n; k++) {
                const float dx = X[i] - bx[k], dy = Y[b] - by[k];
                const float d2 = dx * dx + dy * dy;
                if (d2 >= size2) continue;
                const float I = 1.f - sstep(inner, size, sqrtf(d2));
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
    const float P = TAU / arms, half = wdeg * DEG * 0.5f;
    advance(c, c.s[0], rpm * (TAU / 60.f), TAU);  // in beats: one revolution per cycle
    float maxR = 0;
    for (float cx : {0.f, W})
        for (float cy : {Y[0], Y[0] + H}) maxR = fmaxf(maxR, hypotf(cx - ox, cy - oy));
    const CRGB back = c.lutLin[0];
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        const float dy = Y[b] - oy;
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - ox;
            const float r = sqrtf(dx * dx + dy * dy);
            float a = atan2f(dy, dx) - c.s[0];
            a -= P * roundf(a / P);
            // Near the origin the beam is narrow in metres: keep its edge >= ~0.35 m wide
            const float edge = fmaxf(half * 0.6f, 0.35f / fmaxf(r, 0.2f));
            const float I = 1.f - sstep(fmaxf(half - edge, 0.f), half + edge, fabsf(a));
            const CRGB beam = lutL(c.lutLin, spread * r / maxR);
            o[i] = mixF(scaleF(back, bg), beam, I);
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
    for (int b = 0; b < NY; b++) {
        const float dy = Y[b] - cy;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float dx = X[i] - cx;
            const float r = sqrtf(dx * dx + dy * dy);
            const float sinAbs = r > 1e-3f ? fabsf(dy) / r : 1.f;  // ring crosses the rows at this slant
            const float k = fmaxf(soft, FY[b] * sinAbs / spacing);
            o[i] = bands(r / spacing - c.s[0], k, step, gap, c.lutWrap);
        }
    }
}

// ---------------------------------------------------------------- Drops
// Big rings expanding from random points and fading out, like raindrops on a pond.

struct Drop { float x, y, r; uint8_t col; bool live; };
static Drop drops[10];

static void dropsFx(Ctx& c) {
    const float rate = c.p[0], speed = c.p[1], width = c.p[2], maxR = c.p[3], bg = c.p[4];
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
            d = {frand() * W, Y[0] + frand() * H, 0, random8(), true};
            break;
        }
    }
    for (Drop& d : drops) {
        if (!d.live) continue;
        d.r += speed * c.dt;
        if (d.r > maxR) d.live = false;
    }
    const CRGB back = c.lutLin[0];
    const float sig = width * 0.5f;
    for (int b = 0; b < NY; b++) {
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            float r = back.r * bg, g = back.g * bg, bl = back.b * bg;
            for (const Drop& d : drops) {
                if (!d.live) continue;
                const float dx = X[i] - d.x, dy = Y[b] - d.y;
                const float dist = sqrtf(dx * dx + dy * dy);
                const float off = dist - d.r;
                if (fabsf(off) > 3.f * sig + FY[b]) continue;
                // Widen by the row footprint along the ring's normal, keeping the light energy
                const float fw = FY[b] * (dist > 1e-3f ? fabsf(dy) / dist : 1.f) * 0.29f;
                const float se = sqrtf(sig * sig + fw * fw);
                const float fade = (1.f - d.r / maxR) * fminf(1.f, d.r / width);
                const float I = expf(-off * off / (2.f * se * se)) * (sig / se) * fade;
                const CRGB& k = c.lutWrap[d.col];
                r += k.r * I;
                g += k.g * I;
                bl += k.b * I;
            }
            o[i] = CRGB((uint8_t)fminf(r, 255.f), (uint8_t)fminf(g, 255.f), (uint8_t)fminf(bl, 255.f));
        }
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
    for (int b = 0; b < NY; b++) {
        const float dy1 = Y[b] - y1, dy2 = Y[b] - y2;
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float dx1 = X[i] - x1, dx2 = X[i] - x2;
            const float r1 = sqrtf(dx1 * dx1 + dy1 * dy1), r2 = sqrtf(dx2 * dx2 + dy2 * dy2);
            const float v1 = cosf(TAU * (r1 / lambda - c.s[0])) *
                             footAtten(b, r1 > 1e-3f ? fabsf(dy1) / r1 : 1.f, lambda);
            const float v2 = cosf(TAU * (r2 / lambda - c.s[0])) *
                             footAtten(b, r2 > 1e-3f ? fabsf(dy2) / r2 : 1.f, lambda);
            const float v = (v1 + v2) * 0.25f + 0.5f;  // 0..1
            o[i] = scaleF(lutW(c.lutWrap, c.s[2] + v * 0.5f), 1.f - contrast * (1.f - v));
        }
    }
}

// ---------------------------------------------------------------- Clouds
// Soft cloud shapes drifting on a wind that slowly turns; coverage from clear to overcast.
// Palette as a ramp: first colour = sky, last = cloud.

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
    // Clouds are smooth along a row: sample every ~8 cm and interpolate
    const int ns = clampi((int)(W / 0.08f) + 2, 2, NX);
    static float v[MAX_X];
    for (int b = 0; b < NY; b++) {
        for (int k = 0; k < ns; k++) {
            const float x = X[0] + (X[NX - 1] - X[0]) * k / (ns - 1);
            v[k] = fbm(x - c.s[0], Y[b] - c.s[1], c.s[3], size);
        }
        CRGB* o = c.out + b * NX;
        for (int i = 0; i < NX; i++) {
            const float fk = NX > 1 ? i * (float)(ns - 1) / (NX - 1) : 0;
            int k0 = (int)fk;
            if (k0 >= ns - 1) k0 = ns - 2;
            const float val = v[k0] + (v[k0 + 1] - v[k0]) * (fk - k0);
            o[i] = lutL(c.lutLin, sstep(thr - sw, thr + sw, val));
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

// ---------------------------------------------------------------- table
// New effects go at the end: presets store the index.

// Shared help texts
#define H_BG "How bright the background is where nothing else shines, in the palette's first colour. 0 = black."
#define H_DRIFT "How fast the colours slowly move through the palette. 0 = they stay put."
#define H_GAPS "How dark the borders between neighbouring bands get. 0 = the colours touch, 1 = black between them."
#define H_STEP "How far along the palette each next band moves. Small = neighbours in similar colours, 0.5 = opposite colours in turn, 0 or 1 = every band the same colour."
#define H_SOFT_BANDS "How gradually one band blends into the next. 0 = crisp edges (still softened where they cross the bars, so they don't flicker), 1 = smooth gradients."

const EffectDef EFFECTS[] = {
    {"Rolling bars", 6,
     {{"Speed (m/s)", 0, 1, 0.15f, 0.01f, "How fast the bands roll, in metres per second. 0 stops them; the direction still turns. Calm: 0.05 to 0.2."},
      {"Band width (m)", 0.8f, 5, 2, 0.1f, "The width of one band, in metres. 0.8 m is the narrowest the bar spacing shows without flicker; 5 m is one band across the ceiling's length."},
      {"Softness", 0, 1, 0.6f, 0.01f, H_SOFT_BANDS},
      {"Dark gaps", 0, 1, 0.3f, 0.01f, H_GAPS},
      {"Turn (deg/s)", -10, 10, 1.5f, 0.1f, "How fast the rolling direction turns, in degrees per second; negative turns the other way. 1.5 = a full turn in 4 minutes, 0 = a fixed direction."},
      {"Colour step", 0, 1, 0.2f, 0.01f, H_STEP}},
     rolling, false, "band", 0,
     "Wide soft bands of palette colour rolling across the ceiling while their direction slowly turns. In Beats timing, each band takes the chosen number of beats to pass."},
    {"Searchlights", 5,
     {{"Beams", 1, 5, 3, 1, "How many round beams wander over the ceiling, each in its own palette colour. Where beams overlap their light adds up."},
      {"Beam radius (m)", 0.5f, 3, 1.2f, 0.1f, "The radius of each beam in metres: 1.2 m makes a beam about 2.4 m across."},
      {"Speed", 0, 1.5f, 0.3f, 0.01f, "How fast the beams wander along their looping paths. 0.3 is a slow drift, 1.5 a brisk sweep, 0 freezes them."},
      {"Background", 0, 0.3f, 0.02f, 0.01f, H_BG},
      {"Softness", 0, 1, 0.6f, 0.01f, "How far in from the edge a beam fades. 0 = an evenly lit disc with a short soft edge (never under 0.4 m), 1 = brightest only at the centre."}},
     searchlights, false, nullptr, -1,
     "Soft round beams wandering over the ceiling on slow looping paths, like searchlights in the sky. Their paths don't repeat for a long time."},
    {"Lighthouse", 7,
     {{"Speed (rpm)", -20, 20, 4, 0.1f, "Turns per minute; negative turns the other way. In Beats timing this slider is replaced by beats per revolution."},
      {"Beam width (deg)", 5, 120, 30, 1, "How wide the beam is, as an angle. The beam gets wider in metres the further it is from the centre."},
      {"Beams", 1, 4, 1, 1, "How many beams, evenly spaced around the centre like a lighthouse with several lamps. Each spot is passed this many times per turn."},
      {"Origin X", -0.2f, 1.2f, 0.5f, 0.01f, "Where the centre sits along the bars: 0 = one end, 0.5 = the middle, 1 = the other end. Below 0 or above 1 puts it outside the ceiling."},
      {"Origin Y", -0.2f, 1.2f, -0.05f, 0.01f, "Where the centre sits across the bars: 0 = the first bar, 1 = the last bar. The default, -0.05, is just outside the first bar, so the beam sweeps in from the edge."},
      {"Background", 0, 0.3f, 0.02f, 0.01f, H_BG},
      {"Colour spread", 0, 1, 0.6f, 0.01f, "How the beam's colour changes with distance from the centre. 0 = all in the palette's first colour, 1 = through the whole palette out to the farthest corner."}},
     lighthouse, false, "revolution", 0,
     "A beam turning around a point, like a lighthouse. The point can sit inside the ceiling, at its edge or beyond it. In Beats timing, one revolution takes the chosen number of beats."},
    {"Clouds", 6,
     {{"Coverage", 0, 1, 0.5f, 0.01f, "How much of the sky is cloud: 0 = clear, 0.5 = half cloudy, 1 = overcast."},
      {"Cloud size (m)", 1, 8, 3, 0.1f, "The typical size of a cloud in metres. Smaller clouds still keep their detail coarse enough for the bar spacing."},
      {"Wind (m/s)", 0, 1, 0.15f, 0.01f, "How fast the clouds drift, in metres per second. 0 = they stay in place (they can still evolve)."},
      {"Wind turn (deg/s)", -5, 5, 0.5f, 0.1f, "How fast the wind direction turns, in degrees per second; negative turns the other way. 0 = a steady wind."},
      {"Evolve", 0, 1, 0.2f, 0.01f, "How fast the cloud shapes change as they drift. 0 = rigid shapes sliding past, 1 = clouds forming and dissolving quickly."},
      {"Softness", 0, 1, 0.4f, 0.01f, "How gradually a cloud fades into the sky. 0 = crisp edges, 1 = hazy."}},
     clouds, false, nullptr, -1,
     "Soft clouds drifting on a slowly turning wind. The palette is used as a ramp: its first colour is the sky, its last colour the clouds."},
    {"Sparkle", 4,
     {{"Density", 0, 1, 0.4f, 0.01f, "How many LEDs twinkle at once. 1 = about 8% of the LEDs lit at any moment."},
      {"Twinkle time (s)", 0.3f, 5, 1.5f, 0.1f, "How long one twinkle lasts, in seconds: a quick rise and a slow fade."},
      {"Background", 0, 0.5f, 0.05f, 0.01f, "How bright the slowly drifting palette wash under the twinkles is. 0 = black."},
      {"Drift", 0, 1, 0.2f, 0.01f, "How fast the background wash moves through the palette."}},
     sparkle, true, nullptr, -1,
     "Single LEDs twinkling in palette colours over a dim palette wash. For twinkles on top of another effect, use that preset's Sparkle overlay instead."},
    {"Sweeps", 8,
     {{"Bars", 1, 4, 1, 1, "How many bars travel at once, evenly spaced. Each changes direction on its own."},
      {"Speed (m/s)", 0.05f, 2, 0.4f, 0.01f, "How fast a bar crosses the ceiling, in metres per second. In Beats timing this slider is replaced by beats per sweep."},
      {"Bar width (m)", 0.3f, 4, 1, 0.1f, "The width of the solid part of each bar, in metres."},
      {"Trail (m)", 0, 6, 1.5f, 0.1f, "How far behind the bar its glow fades out, in metres. 0 = no trail."},
      {"Angle change (deg)", 0, 180, 37, 1, "How much the direction turns after each pass. 180 = back and forth, 90 = crosswise in turn, 0 = always the same way. Odd angles such as 37 rarely repeat."},
      {"Random angle", 0, 1, 0, 1, "1 = each pass goes in a random direction, ignoring Angle change."},
      {"Background", 0, 0.3f, 0.02f, 0.01f, H_BG},
      {"Softness", 0, 1, 0.2f, 0.01f, "How soft the bar's edges are, as a part of its width. 0 = crisp (still softened where they cross the bars)."}},
     sweeps, false, "sweep", 1,
     "Bars sweeping across the ceiling one after another, each pass at a new angle, with a fading trail. Each new pass takes the next palette colour."},
    {"Plaid", 6,
     {{"Stripe period (m)", 1, 6, 2.5f, 0.1f, "The distance from one stripe to the next, in metres."},
      {"Speed (m/s)", 0, 1, 0.1f, 0.01f, "How fast the stripes move, in metres per second; the two sets move opposite ways. In Beats timing this slider is replaced by beats per stripe."},
      {"Turn (deg/s)", -10, 10, 1, 0.1f, "How fast the whole pattern turns, in degrees per second; negative turns the other way."},
      {"Cross angle (deg)", 0, 90, 60, 1, "The angle between the two stripe sets. 90 = square checks, smaller = stretched diamonds, 0 = both sets parallel."},
      {"Contrast", 0, 1, 0.6f, 0.01f, "How dark it gets between the stripes. 0 = even brightness, only the colours change, 1 = deep dark gaps."},
      {"Colour drift", 0, 1, 0.2f, 0.01f, H_DRIFT}},
     plaid, false, "stripe", 1,
     "Two sets of broad stripes crossing at an angle, moving and slowly turning. The colour shows which of the two sets is stronger at each spot."},
    {"Rings", 7,
     {{"Ring spacing (m)", 0.8f, 4, 1.5f, 0.1f, "The distance from one ring to the next, in metres."},
      {"Speed (m/s, - = inward)", -1, 1, 0.2f, 0.01f, "How fast the rings flow, in metres per second: positive outward from the centre, negative inward. In Beats timing this slider is replaced by beats per ring."},
      {"Softness", 0, 1, 0.6f, 0.01f, H_SOFT_BANDS},
      {"Dark gaps", 0, 1, 0.4f, 0.01f, H_GAPS},
      {"Centre wander", 0, 1, 0.4f, 0.01f, "How far the centre roams. 0 = fixed in the middle of the ceiling, 1 = all the way to the edges."},
      {"Wander speed", 0, 1, 0.2f, 0.01f, "How fast the centre roams."},
      {"Colour step", 0, 1, 0.2f, 0.01f, H_STEP}},
     rings, false, "ring", 1,
     "Concentric rings flowing out of (or into) a centre that slowly wanders over the ceiling."},
    {"Drops", 5,
     {{"Drops per minute", 1, 60, 10, 1, "How many new drops fall per minute, at random places. At most 10 rings show at once. In Beats timing this slider is replaced by beats per drop, and each drop lands on the beat."},
      {"Speed (m/s)", 0.1f, 2, 0.5f, 0.01f, "How fast a ring spreads, in metres per second."},
      {"Ring width (m)", 0.4f, 2.5f, 1, 0.1f, "The width of a ring's bright band, in metres."},
      {"Max radius (m)", 1, 12, 6, 0.1f, "How far a ring spreads before it has faded out completely, in metres."},
      {"Background", 0, 0.3f, 0.03f, 0.01f, H_BG}},
     dropsFx, false, "drop", 0,
     "Big rings spreading from random points and fading out, like raindrops on a pond. Each drop takes a random palette colour."},
    {"Interference", 6,
     {{"Wavelength (m)", 0.8f, 5, 1.8f, 0.1f, "The distance from one wave to the next, in metres."},
      {"Speed (m/s)", -1, 1, 0.15f, 0.01f, "How fast the waves travel, in metres per second: positive outward, negative inward. In Beats timing this slider is replaced by beats per wave."},
      {"Centre distance", 0, 1, 0.5f, 0.01f, "How far apart the two wave centres are. 0 = the same point (plain rings), 1 = about the ceiling's size apart."},
      {"Rotate (deg/s)", -10, 10, 2, 0.1f, "How fast the two centres circle each other, in degrees per second; negative the other way."},
      {"Contrast", 0, 1, 0.7f, 0.01f, "How dark the troughs get where the waves cancel. 0 = even brightness, 1 = black."},
      {"Colour drift", 0, 1, 0.2f, 0.01f, H_DRIFT}},
     interference, false, "wave", 1,
     "Two sets of ring waves around centres that slowly circle each other. Where the waves meet they reinforce or cancel, forming large moving patterns."},
};const int NUM_EFFECTS = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

void effectsInit() {
    NX = layout.nx;
    NY = layout.ny;
    NL = NX * NY;
    const float pitch = layout.length / NX;
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
