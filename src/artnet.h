// Art-Net output in the PixLite's universe map: bar n = universe 2n (pixels 1-170, 510
// channels) + universe 2n+1 (pixels 171-180, 30 channels).
#pragma once
#include <FastLED.h>
#include <IPAddress.h>
#include <WiFiUdp.h>

class ArtNetOut {
public:
    // lut: per-channel output curve (gamma and brightness)
    void send(IPAddress to, const CRGB* leds, const uint8_t* lut);

private:
    void sendUniverse(IPAddress to, uint16_t universe, const uint8_t* data, uint16_t len,
                      const uint8_t* lut);
    WiFiUDP udp;
    uint8_t seq = 1;
    uint8_t buf[18 + 512];
};
