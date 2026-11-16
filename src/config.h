// Installation defaults. Positions mirror data/layout.json.
#pragma once
#include <stdint.h>

namespace cfg {
// The ceiling: 15 bars of 180 LEDs, 5 m long, rows measured (bar 15 estimated)
constexpr int CEILING_ROWS = 15;
constexpr int CEILING_PIX = 180;
constexpr float CEILING_LENGTH = 5.0f;
extern const float CEILING_Y[CEILING_ROWS];

constexpr int FPS = 40;

// Default fallback address when no DHCP server answers (the cable goes only to the PixLite)
constexpr uint8_t ETH_IP[4] = {192, 168, 0, 10};
constexpr uint8_t DEFAULT_TARGET[4] = {192, 168, 0, 20};  // Beam; PixLite is 192.168.0.50

constexpr int BUTTON_PIN = 4;          // to GND; short press next preset, long press on/off

// OSC in (UDP) and OSCQuery (HTTP + WebSocket, for MIMa and other OSCQuery clients)
constexpr uint16_t OSC_PORT = 8000;
constexpr uint16_t OSCQUERY_PORT = 8010;
}  // namespace cfg
