#include "effects.h"

#include <math.h>

using namespace cfg;

static constexpr float DEG = 0.01745329f;
static constexpr float TAU = 6.2831853f;

static float X[PIX];   // m along a bar
static float Y[BARS];  // m across the bars
static float FY[BARS]; // footprint of a bar across the bars (m)
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

// Fractal noise, roughly -1..1. Octaves finer than 0.8 m are left out (they'd flicker
// across the bars).
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

static uint16_t spPhase[NUM];  // 0 = idle, else 1..65535 through one twinkle
static uint8_t spColour[NUM];

static void sparkleStep(float density, float life, float dt) {
    // density 1 = about 8% of the LEDs lit at any moment
    const float prob = density * 0.08f / life * dt;
    const uint16_t th = (uint16_t)fminf(prob * 65535.f, 65535.f);
    const uint32_t inc = (uint32_t)fmaxf(1.f, 65535.f * dt / life);
    for (int i = 0; i < NUM; i++) {
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
    for (int i = 0; i < NUM; i++) {
        if (!spPhase[i]) continue;
        uint8_t a = (uint8_t)(sparkleLevel(spPhase[i]) * 255.f);
        out[i] += CRGB(a, a, a);
    }
}

// ---------------------------------------------------------------- Rolling bars
// Wide bands rolling across the ceiling; their direction slowly turns.

static void rolling(Ctx& c) {
    const float speed = c.p[0], width = c.p[1], soft = c.p[2], gap = c.p[3], turn = c.p[4],
                step = c.p[5];
    c.s[0] += c.dt * turn * DEG;
    c.s[1] += c.dt * speed / width;
    const float ca = cosf(c.s[0]), sa = sinf(c.s[0]);
    for (int b = 0; b < BARS; b++) {
        // Transition width (in bands): asked softness, but at least the bar's footprint
        // projected onto the roll direction
        const float k = fminf(1.f, fmaxf(0.03f, fmaxf(soft, FY[b] * fabsf(sa) / width)));
        const float dy = (Y[b] - CY) * sa;
        CRGB* o = c.out + b * PIX;
        for (int i = 0; i < PIX; i++) {
            const float u = ((X[i] - CX) * ca + dy) / width - c.s[1];
            const float n = floorf(u), f = u - n;  // band n, centre at f = 0, edge at 0.5
            const float cpos = (n + sstep(0.5f - k * 0.5f, 0.5f + k * 0.5f, f)) * step;
            const float w = 1.f - sstep(0.f, k * 0.5f + 0.001f, fabsf(f - 0.5f));
            o[i] = scaleF(lutW(c.lutWrap, cpos), 1.f - gap * w);
        }
    }
}

// ---------------------------------------------------------------- Searchlights
// Soft round beams wandering over the ceiling on slow Lissajous paths.

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
    for (int b = 0; b < BARS; b++) {
        CRGB* o = c.out + b * PIX;
        for (int i = 0; i < PIX; i++) {
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
// A beam turning around a point at (or beyond) the edge of the ceiling.

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
    for (int b = 0; b < BARS; b++) {
        CRGB* o = c.out + b * PIX;
        const float dy = Y[b] - oy;
        for (int i = 0; i < PIX; i++) {
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
    // Clouds are smooth along a bar: sample every ~8 cm and interpolate
    constexpr int NS = 61;
    float v[NS];
    for (int b = 0; b < BARS; b++) {
        for (int k = 0; k < NS; k++) {
            const float x = MARGIN + k * (float)(PIX - 1) / (NS - 1) * PITCH;
            v[k] = fbm(x - c.s[0], Y[b] - c.s[1], c.s[3], size);
        }
        CRGB* o = c.out + b * PIX;
        for (int i = 0; i < PIX; i++) {
            const float fk = i * (float)(NS - 1) / (PIX - 1);
            int k0 = (int)fk;
            if (k0 >= NS - 1) k0 = NS - 2;
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
    for (int b = 0; b < BARS; b++) {
        CRGB* o = c.out + b * PIX;
        for (int i = 0; i < PIX; i++) {
            const int idx = b * PIX + i;
            const CRGB base = scaleF(lutW(c.lutWrap, X[i] / W * 0.5f + Y[b] / H * 0.3f + c.s[0]), bg);
            if (!spPhase[idx]) { o[i] = base; continue; }
            o[i] = mixF(base, c.lutWrap[spColour[idx]], sparkleLevel(spPhase[idx]));
        }
    }
}

// ---------------------------------------------------------------- table

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
};
const int NUM_EFFECTS = sizeof(EFFECTS) / sizeof(EFFECTS[0]);

void effectsInit() {
    for (int i = 0; i < PIX; i++) X[i] = MARGIN + i * PITCH;
    for (int b = 0; b < BARS; b++) Y[b] = BAR_Y[b];
    for (int b = 0; b < BARS; b++) {
        const float lo = b > 0 ? Y[b - 1] : Y[b] - (Y[b + 1] - Y[b]);
        const float hi = b < BARS - 1 ? Y[b + 1] : Y[b] + (Y[b] - Y[b - 1]);
        FY[b] = (hi - lo) * 0.5f;
    }
    W = LENGTH;
    H = Y[BARS - 1] - Y[0];
    CX = W * 0.5f;
    CY = Y[0] + H * 0.5f;

    // Scale the noise so a single octave has a standard deviation of ~0.45
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
