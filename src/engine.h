// Playback state shared between the render loop, the button and the web page.
#pragma once

void engineSelect(int preset);   // fades over to the preset (and switches on)
void engineSetOn(bool on);       // fades in / out
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
