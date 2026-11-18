// Art-Net output of the LED grid, one universe range per row (see artnet.cpp). The packets go
// out from a task of their own on core 0 (with the network stack), so the render loop hands a
// frame over and carries on instead of waiting for 30 packets to be sent.
#pragma once
#include <FastLED.h>
#include <IPAddress.h>

#include "layout.h"

class ArtNetOut {
public:
    void begin();  // starts the sender task
    // Takes the frame (the per-channel output curve, gamma and brightness, applied now) and
    // returns; the task sends it. False when the last frame is still going out: this one is
    // dropped rather than queued.
    bool send(IPAddress to, const CRGB* leds, const uint8_t* lut);
    float sendMs() const { return ms; }  // time the task takes per frame, smoothed
    uint32_t sent = 0, failed = 0;  // packets; failed: sendto refused them
    int lastErrno = 0;               // why the last one failed
    uint32_t skipped = 0;            // frames not sent: target out of reach (see netCanSendTo)
    uint32_t dropped = 0;            // frames not sent: the last one was still going out
    // Longest wait from hand-over to the task starting, and longest send, in microseconds
    // since the last takeTiming()
    void takeTiming(uint32_t& waitMaxUs, uint32_t& sendMaxUs) {
        waitMaxUs = waitMax; sendMaxUs = sendMax; waitMax = sendMax = 0;
    }

private:
    static void task(void* self);
    void sendAll();
    void sendUniverse(uint16_t universe, const uint8_t* data, uint16_t len);

    TaskHandle_t handle = nullptr;
    volatile bool busy = false;
    volatile float ms = 0;
    volatile uint32_t handedAt = 0, waitMax = 0, sendMax = 0;
    IPAddress target;
    uint16_t nx = 0, ny = 0, rowUni[MAX_Y];  // the layout of the frame being sent
    uint8_t order[MAX_Y];                   // and its colour order per row
    uint8_t seq = 1;
    uint8_t* frame = nullptr;  // the layout's LEDs * 3, from the heap (static DRAM is full)
    size_t frameCap = 0;
    uint8_t buf[18 + 512];
};
