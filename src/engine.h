// Playback state shared between the render loop, the button and the web page.
#pragma once

void engineSelect(int preset);   // fades over to the preset (and switches on)
void engineSetOn(bool on);       // fades in / out
bool engineOn();
void engineResetEffect();        // the current preset's effect changed
float engineFps();
float engineRenderMs();
int engineCurrent();              // preset playing, or being faded to
void engineLayoutChanged();      // after editing `layout`: re-init positions, save
