// Installation constants. Positions mirror data/layout.json.
#pragma once
#include <stdint.h>

namespace cfg {
constexpr int BARS = 15;
constexpr int PIX = 180;               // pixels per bar
constexpr int NUM = BARS * PIX;        // 2700
constexpr float PITCH = 0.0276f;       // m between pixels along a bar
constexpr float MARGIN = 0.02f;        // m from bar end to first pixel
constexpr float LENGTH = MARGIN * 2 + PITCH * (PIX - 1);
extern const float BAR_Y[BARS];        // m, measured (bar 15 estimated)

constexpr int FPS = 40;

// Art-Net: bar n = universe 2n (pixels 1-170) + first 30 channels of universe 2n+1.
constexpr int PIX_FIRST_UNI = 170;

// Default fallback address when no DHCP server answers (the cable goes only to the PixLite)
constexpr uint8_t ETH_IP[4] = {192, 168, 0, 10};
constexpr uint8_t DEFAULT_TARGET[4] = {192, 168, 0, 20};  // Beam; PixLite is 192.168.0.50

constexpr int BUTTON_PIN = 4;          // to GND; short press next preset, long press on/off
}  // namespace cfg
