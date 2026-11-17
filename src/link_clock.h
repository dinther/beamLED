// Ableton Link: tempo, beat and bar shared with Ableton Live and other Link apps on the network.
// Ported from Glow's LinkClock. With no peers (or Link off) it keeps running at its own tempo,
// so everything tied to beats still works. The Link SDK is used under Ableton's licence
// agreement: unmodified (fetched at build time by scripts/fetch_link.py), and the page names it
// "Ableton Link" in full with its state as Enabled / Disabled, as Ableton's guidelines require.
#pragma once

#include <memory>

struct BeatTime {
    double bpm = 120;
    double beat = 0;     // beats since the session's origin
    double phase = 0;    // 0..quantum within the bar
    double quantum = 4;  // beats per bar
    int peers = 0;       // other Link apps in the session
    bool link = false;   // Link is on
};

class LinkClock {
public:
    explicit LinkClock(double bpm);
    ~LinkClock();
    LinkClock(const LinkClock&) = delete;
    LinkClock& operator=(const LinkClock&) = delete;

    void enable(bool on);
    // `aheadMs`: where the music will be that much later, so a frame that takes that long to
    // reach the LEDs lands on the beat
    BeatTime now(double aheadMs = 0) const;
    void setTempo(double bpm);  // also changes it for every peer
    void setBeat(double beat);  // carry on from the board's own beat count (before Link existed)
    // Starts Link's discovery afresh (a running Link sometimes stays alone while other apps
    // see each other); tempo and beat carry on
    void rejoin();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
