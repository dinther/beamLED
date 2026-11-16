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
    c.s[1] += c.dt * speed / width;
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
    c.s[0] += c.dt * speed;
    float pos[4], ca[4], sa[4];
    CRGB col[4];
    for (int k = 0; k < n; k++) {
        const float p = fmodf(c.s[0] + k * L / n, L);
        if (p < sweepPrev[k] && sweepPrev[k] < 1e8f) {  // wrapped: next pass, new angle
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
    c.s[1] += c.dt * speed / period;
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
    c.s[0] += c.dt * rpm * (TAU / 60.f);
    const float P = TAU / arms, half = wdeg * DEG * 0.5f;
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
    c.s[0] += c.dt * speed / spacing;
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
    c.s[0] += c.dt * rate / 60.f;
    while (c.s[0] >= 1.f) {
        c.s[0] -= 1.f;
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
    c.s[0] += c.dt * speed / lambda;
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

const EffectDef EFFECTS[] = {
    {"Rolling bars", 6,
     {{"Speed (m/s)", 0, 1, 0.15f, 0.01f},
      {"Band width (m)", 0.8f, 5, 2, 0.1f},
      {"Softness", 0, 1, 0.6f, 0.01f},
      {"Dark gaps", 0, 1, 0.3f, 0.01f},
      {"Turn (deg/s)", -10, 10, 1.5f, 0.1f},
      {"Colour step", 0, 1, 0.2f, 0.01f}},
     rolling, false},
    {"Searchlights", 5,
     {{"Beams", 1, 5, 3, 1},
      {"Beam radius (m)", 0.5f, 3, 1.2f, 0.1f},
      {"Speed", 0, 1.5f, 0.3f, 0.01f},
      {"Background", 0, 0.3f, 0.02f, 0.01f},
      {"Softness", 0, 1, 0.6f, 0.01f}},
     searchlights, false},
    {"Lighthouse", 7,
     {{"Speed (rpm)", -20, 20, 4, 0.1f},
      {"Beam width (deg)", 5, 120, 30, 1},
      {"Beams", 1, 4, 1, 1},
      {"Origin X", -0.2f, 1.2f, 0.5f, 0.01f},
      {"Origin Y", -0.2f, 1.2f, -0.05f, 0.01f},
      {"Background", 0, 0.3f, 0.02f, 0.01f},
      {"Colour spread", 0, 1, 0.6f, 0.01f}},
     lighthouse, false},
    {"Clouds", 6,
     {{"Coverage", 0, 1, 0.5f, 0.01f},
      {"Cloud size (m)", 1, 8, 3, 0.1f},
      {"Wind (m/s)", 0, 1, 0.15f, 0.01f},
      {"Wind turn (deg/s)", -5, 5, 0.5f, 0.1f},
      {"Evolve", 0, 1, 0.2f, 0.01f},
      {"Softness", 0, 1, 0.4f, 0.01f}},
     clouds, false},
    {"Sparkle", 4,
     {{"Density", 0, 1, 0.4f, 0.01f},
      {"Twinkle time (s)", 0.3f, 5, 1.5f, 0.1f},
      {"Background", 0, 0.5f, 0.05f, 0.01f},
      {"Drift", 0, 1, 0.2f, 0.01f}},
     sparkle, true},
    {"Sweeps", 8,
     {{"Bars", 1, 4, 1, 1},
      {"Speed (m/s)", 0.05f, 2, 0.4f, 0.01f},
      {"Bar width (m)", 0.3f, 4, 1, 0.1f},
      {"Trail (m)", 0, 6, 1.5f, 0.1f},
      {"Angle change (deg)", 0, 180, 37, 1},
      {"Random angle", 0, 1, 0, 1},
      {"Background", 0, 0.3f, 0.02f, 0.01f},
      {"Softness", 0, 1, 0.2f, 0.01f}},
     sweeps, false},
    {"Plaid", 6,
     {{"Stripe period (m)", 1, 6, 2.5f, 0.1f},
      {"Speed (m/s)", 0, 1, 0.1f, 0.01f},
      {"Turn (deg/s)", -10, 10, 1, 0.1f},
      {"Cross angle (deg)", 0, 90, 60, 1},
      {"Contrast", 0, 1, 0.6f, 0.01f},
      {"Colour drift", 0, 1, 0.2f, 0.01f}},
     plaid, false},
    {"Rings", 7,
     {{"Ring spacing (m)", 0.8f, 4, 1.5f, 0.1f},
      {"Speed (m/s, - = inward)", -1, 1, 0.2f, 0.01f},
      {"Softness", 0, 1, 0.6f, 0.01f},
      {"Dark gaps", 0, 1, 0.4f, 0.01f},
      {"Centre wander", 0, 1, 0.4f, 0.01f},
      {"Wander speed", 0, 1, 0.2f, 0.01f},
      {"Colour step", 0, 1, 0.2f, 0.01f}},
     rings, false},
    {"Drops", 5,
     {{"Drops per minute", 1, 60, 10, 1},
      {"Speed (m/s)", 0.1f, 2, 0.5f, 0.01f},
      {"Ring width (m)", 0.4f, 2.5f, 1, 0.1f},
      {"Max radius (m)", 1, 12, 6, 0.1f},
      {"Background", 0, 0.3f, 0.03f, 0.01f}},
     dropsFx, false},
    {"Interference", 6,
     {{"Wavelength (m)", 0.8f, 5, 1.8f, 0.1f},
      {"Speed (m/s)", -1, 1, 0.15f, 0.01f},
      {"Centre distance", 0, 1, 0.5f, 0.01f},
      {"Rotate (deg/s)", -10, 10, 2, 0.1f},
      {"Contrast", 0, 1, 0.7f, 0.01f},
      {"Colour drift", 0, 1, 0.2f, 0.01f}},
     interference, false},
};
const int NUM_EFFECTS = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

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
