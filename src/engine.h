// Playback state shared between the render loop, the button and the web page.
#pragma once

// The presets, palettes and settings are changed by the web API (on the HTTP server's task) and
// used by the render loop: whoever changes or renders holds the lock. Recursive.
void engineLock();
void engineUnlock();
struct EngineLock {
    EngineLock() { engineLock(); }
    ~EngineLock() { engineUnlock(); }
    EngineLock(const EngineLock&) = delete;
    EngineLock& operator=(const EngineLock&) = delete;
};

void engineSelect(int preset);   // fades over to the preset (and switches on)
void engineSetOn(bool on);       // fades in / out
float engineAutoLeft();          // Auto: until the next change, in seconds or bars; -1 when not running
bool engineDeletePreset(int i);  // the preset list changes: the engine keeps its place
bool engineMovePreset(int from, int to);
bool engineOn();
void engineResetEffect();        // the current preset's effect changed
float engineFps();
float engineRenderMs();
float engineSendMs();      // Art-Net out, per frame
int engineCurrent();              // preset playing, or being faded to
void engineLayoutChanged();      // after editing `layout`: re-init positions, save

struct BeatTime;
const BeatTime& engineBeat();    // Ableton Link: tempo, beat, peers
void engineSetTempo(double bpm); // own tempo; with Ableton Link enabled, everyone's
void engineMotion(int m, float& phase, float& rate, uint32_t& cycle);  // the playing preset's motion m now
