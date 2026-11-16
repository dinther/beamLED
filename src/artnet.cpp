#include "artnet.h"

#include "config.h"

void ArtNetOut::sendUniverse(IPAddress to, uint16_t universe, const uint8_t* data, uint16_t len,
                             const uint8_t* lut) {
    memcpy(buf, "Art-Net", 8);
    buf[8] = 0x00;  // OpDmx 0x5000, little endian
    buf[9] = 0x50;
    buf[10] = 0;    // protocol version 14
    buf[11] = 14;
    buf[12] = seq;
    buf[13] = 0;    // physical
    buf[14] = universe & 0xFF;          // SubUni
    buf[15] = (universe >> 8) & 0x7F;   // Net
    buf[16] = len >> 8;
    buf[17] = len & 0xFF;
    for (uint16_t i = 0; i < len; i++) buf[18 + i] = lut[data[i]];
    udp.beginPacket(to, 6454);
    udp.write(buf, 18 + len);
    udp.endPacket();
}

void ArtNetOut::send(IPAddress to, const CRGB* leds, const uint8_t* lut) {
    using namespace cfg;
    for (int b = 0; b < BARS; b++) {
        const uint8_t* bar = (const uint8_t*)&leds[b * PIX];
        sendUniverse(to, 2 * b, bar, PIX_FIRST_UNI * 3, lut);
        sendUniverse(to, 2 * b + 1, bar + PIX_FIRST_UNI * 3, (PIX - PIX_FIRST_UNI) * 3, lut);
    }
    if (++seq == 0) seq = 1;  // 0 means "sequence not used"
}
