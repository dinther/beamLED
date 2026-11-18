#include "palette.h"

#include <math.h>

void paletteEvenStops(Palette& p) {
    const int n = p.n < 1 ? 1 : (p.n > PAL_MAX ? PAL_MAX : p.n);
    for (int j = 0; j < PAL_MAX; j++) p.pos[j] = n > 1 ? (j < n ? (float)j / (n - 1) : 1.f) : 0.f;
}

bool paletteHasEvenStops(const Palette& p) {
    const int n = p.n < 1 ? 1 : (p.n > PAL_MAX ? PAL_MAX : p.n);
    for (int j = 0; j < n; j++)
        if (fabsf(p.pos[j] - (n > 1 ? (float)j / (n - 1) : 0.f)) > 1e-4f) return false;
    return true;
}

void paletteSanitizeStops(Palette& p) {
    float last = 0;
    for (int j = 0; j < PAL_MAX; j++) {
        float v = p.pos[j];
        if (!(v >= 0)) v = 0;  // also NaN
        if (v > 1) v = 1;
        if (v < last) v = last;
        p.pos[j] = last = v;
    }
}

void paletteSanitizeMids(Palette& p) {
    for (uint8_t& m : p.mid) m = m == 0 ? 128 : (m < 5 ? 5 : (m > 251 ? 251 : m));
}

// How far the blend has gone at f (0..1 of the stretch) with its midpoint at m/256: f itself at
// the middle, else bent so that it is half-way at m. Above the middle the curve of a CSS colour
// hint (f^k); below it that curve's mirror image (1 - (1-f)^k of the mirrored midpoint), so a
// midpoint and its mirror (0.16 and 0.84) give mirror-image blends.
static inline float bias(float f, uint8_t m) {
    if (m == 128 || m == 0 || f <= 0.f || f >= 1.f) return f;
    if (m > 128) return powf(f, -0.6931472f / logf(m / 256.f));
    return 1.f - powf(1.f - f, -0.6931472f / logf((256 - m) / 256.f));
}

static inline CRGB mix(const CRGB& A, const CRGB& B, float f) {
    return CRGB((uint8_t)(A.r + (B.r - A.r) * f + 0.5f), (uint8_t)(A.g + (B.g - A.g) * f + 0.5f),
                (uint8_t)(A.b + (B.b - A.b) * f + 0.5f));
}

void buildLut(const Palette& p, CRGB* lut, bool wrap) {
    const int n = p.n < 1 ? 1 : (p.n > PAL_MAX ? PAL_MAX : p.n);
    if (n == 1 || paletteHasEvenStops(p)) {
        // Evenly spaced: exactly as before colours had positions
        for (int i = 0; i < 256; i++) {
            float pos = wrap ? i / 256.f * n : i / 255.f * (n - 1);
            int a = (int)pos;
            float f = pos - a;
            int b;
            if (wrap) { a %= n; b = (a + 1) % n; }
            else { if (a >= n - 1) { a = n - 1; f = 0; } b = a + 1 < n ? a + 1 : a; }
            // the loop's way back from the last colour to the first has no midpoint of its own
            lut[i] = mix(p.c[a], p.c[b], a < n - 1 ? bias(f, p.mid[a]) : f);
        }
        return;
    }
    // Stops where the palette says. For the loop they take the first (n-1)/n, and the last colour
    // blends back into the first (at 1) over what is left, as with even spacing.
    float t[PAL_MAX + 1];
    const float squeeze = wrap ? (float)(n - 1) / n : 1.f;
    for (int j = 0; j < n; j++) t[j] = p.pos[j] * squeeze;
    for (int i = 0; i < 256; i++) {
        const float x = wrap ? i / 256.f : i / 255.f;
        if (!wrap && x <= t[0]) { lut[i] = p.c[0]; continue; }
        if (x >= t[n - 1]) {
            if (!wrap) { lut[i] = p.c[n - 1]; continue; }
            // last colour -> first colour, from t[n-1] round to 1 + t[0]
            const float span = 1.f + t[0] - t[n - 1];
            lut[i] = mix(p.c[n - 1], p.c[0], span > 1e-6f ? (x - t[n - 1]) / span : 0.f);
            continue;
        }
        if (wrap && x < t[0]) {  // before the first stop: still on the way from the last colour
            const float span = 1.f + t[0] - t[n - 1];
            lut[i] = mix(p.c[n - 1], p.c[0], span > 1e-6f ? (x + 1.f - t[n - 1]) / span : 1.f);
            continue;
        }
        int j = 0;
        while (j < n - 2 && x >= t[j + 1]) j++;
        const float span = t[j + 1] - t[j];
        lut[i] = mix(p.c[j], p.c[j + 1], span > 1e-6f ? bias((x - t[j]) / span, p.mid[j]) : 1.f);
    }
}
