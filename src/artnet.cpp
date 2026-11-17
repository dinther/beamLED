#include "artnet.h"

#include <lwip/sockets.h>

#include "osc.h"

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
    memcpy(order, rowOrder, sizeof(order));
    const uint8_t* in = (const uint8_t*)leds;
    // The output curve, and each row's colour order (where R, G and B go in the packet)
    static const uint8_t SLOT[NUM_ORDERS][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {2, 0, 1}, {1, 2, 0}, {2, 1, 0}};
    for (int b = 0; b < ny; b++) {
        const uint8_t* s = SLOT[order[b] < NUM_ORDERS ? order[b] : 0];
        const int base = b * nx * 3;
        for (int i = 0; i < nx; i++) {
            const int k = base + i * 3;
            frame[k + s[0]] = lut[in[k]];
            frame[k + s[1]] = lut[in[k + 1]];
            frame[k + s[2]] = lut[in[k + 2]];
        }
    }
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
    // From the OSC socket: one UDP socket fewer (the ESP32 has 16 in all)
    const int s = oscUdpSocket();
    if (s < 0) return;
    sockaddr_in to = {};
    to.sin_family = AF_INET;
    to.sin_port = htons(6454);
    to.sin_addr.s_addr = (uint32_t)target;
    if (sendto(s, buf, 18 + len, 0, (sockaddr*)&to, sizeof(to)) < 0) {
        failed++;
        lastErrno = errno;
    } else {
        sent++;
    }
}

// Each row starts at its own universe (layout.rowUni) and fills 170 RGB pixels per universe. The
// default: row n at universe 2n, so a row of up to 340 LEDs fits (pixels 1-170, then 171-340 in 2n+1).
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
