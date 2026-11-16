#include "artnet.h"

void ArtNetOut::begin() {
    if (!frame) frame = (uint8_t*)malloc(MAX_LEDS * 3);
    if (frame && !handle) xTaskCreatePinnedToCore(task, "artnet", 4096, this, 3, &handle, 0);
}

bool ArtNetOut::send(IPAddress to, const CRGB* leds, const uint8_t* lut) {
    if (busy || !handle) return false;
    target = to;
    nx = layout.nx;
    ny = layout.ny;
    memcpy(rowUni, layout.rowUni, sizeof(rowUni));
    const uint8_t* in = (const uint8_t*)leds;
    const int n = nx * ny * 3;
    for (int i = 0; i < n; i++) frame[i] = lut[in[i]];
    busy = true;
    xTaskNotifyGive(handle);
    return true;
}

void ArtNetOut::task(void* self) {
    ArtNetOut& a = *(ArtNetOut*)self;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t t0 = micros();
        a.sendAll();
        a.ms = a.ms + ((micros() - t0) / 1000.f - a.ms) * 0.05f;
        a.busy = false;
    }
}

void ArtNetOut::sendUniverse(uint16_t universe, const uint8_t* data, uint16_t len) {
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
    memcpy(buf + 18, data, len);
    if (len & 1) buf[18 + len++] = 0;   // Art-Net wants an even length
    udp.beginPacket(target, 6454);
    udp.write(buf, 18 + len);
    udp.endPacket();
}

// Each row starts at its own universe (layout.rowUni) and fills 170 RGB pixels per universe. The
// ceiling's PixLite: 180 LEDs per row, row n = universe 2n (pixels 1-170) + 2n+1 (pixels 171-180).
void ArtNetOut::sendAll() {
    const int upr = (nx + 169) / 170;
    for (int b = 0; b < ny; b++) {
        const uint8_t* row = frame + b * nx * 3;
        for (int u = 0; u < upr; u++) {
            const int first = u * 170, count = min(170, nx - first);
            sendUniverse(rowUni[b] + u, row + first * 3, count * 3);
        }
    }
    if (++seq == 0) seq = 1;  // 0 means "sequence not used"
}
