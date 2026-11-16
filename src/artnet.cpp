#include "artnet.h"

#include "layout.h"

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
    if (len & 1) buf[18 + len++] = 0;   // Art-Net wants an even length
    udp.beginPacket(to, 6454);
    udp.write(buf, 18 + len);
    udp.endPacket();
}

// Each row starts on a new universe (170 RGB pixels per universe), as the PixLite is set up:
// with 180 LEDs per row, row n = universe start + 2n (pixels 1-170) and + 2n+1 (pixels 171-180).
void ArtNetOut::send(IPAddress to, const CRGB* leds, const uint8_t* lut) {
    const int nx = layout.nx, upr = universesPerRow();
    for (int b = 0; b < layout.ny; b++) {
        const uint8_t* row = (const uint8_t*)&leds[b * nx];
        for (int u = 0; u < upr; u++) {
            const int first = u * 170, count = min(170, nx - first);
            sendUniverse(to, layout.startUni + b * upr + u, row + first * 3, count * 3, lut);
        }
    }
    if (++seq == 0) seq = 1;  // 0 means "sequence not used"
}
