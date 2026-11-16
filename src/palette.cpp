#include "palette.h"

void buildLut(const Palette& p, CRGB* lut, bool wrap) {
    const int n = p.n < 1 ? 1 : (p.n > PAL_MAX ? PAL_MAX : p.n);
    for (int i = 0; i < 256; i++) {
        float pos = wrap ? i / 256.f * n : i / 255.f * (n - 1);
        int a = (int)pos;
        float f = pos - a;
        int b;
        if (wrap) { a %= n; b = (a + 1) % n; }
        else { if (a >= n - 1) { a = n - 1; f = 0; } b = a + 1 < n ? a + 1 : a; }
        const CRGB& A = p.c[a];
        const CRGB& B = p.c[b];
        lut[i] = CRGB((uint8_t)(A.r + (B.r - A.r) * f + 0.5f),
                      (uint8_t)(A.g + (B.g - A.g) * f + 0.5f),
                      (uint8_t)(A.b + (B.b - A.b) * f + 0.5f));
    }
}
