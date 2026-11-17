// OSC control with OSCQuery (https://github.com/Vidvox/OSCQueryProposal), made for MIMa.
//
// OSC in: UDP port cfg::OSC_PORT, or binary frames on the WebSocket (the web page's sliders). OSCQuery: HTTP on cfg::OSCQUERY_PORT lists every address with
// its type, range, description and current value; HOST_INFO points clients at the OSC port.
// LISTEN: a WebSocket on the same port; a client sends {"COMMAND":"LISTEN","DATA":"/beamled/on"}
// (or IGNORE; a container covers everything under it) and gets each change of that address as
// a binary frame holding one OSC message (no value straight away: the current value is at
// ?VALUE, and MIMa's Learn must hear only real changes). That is the feedback: knobs and lights
// follow the web page and the button. PATH_ADDED / PATH_REMOVED
// (text) go out when addresses come and go (a preset added, deleted, or given another effect).
//
// Addresses (presets and palettes numbered from 1, as on the web page):
//   /beamled/preset i, /beamled/next, /beamled/prev, /beamled/on T, /beamled/brightness f 0..1,
//   /beamled/link T, /beamled/save (no tempo: Ableton Link moves it by fractions of a BPM all the
//   time, which would reach every listener as a stream of changes; set it on the web page or in Live)
//   /beamled/presets/<n>/play T | effect i | palette i | sparkle f | timing i | beats i |
//                        reverse T | pulse f | sparkle_colour i (0 white, 1 own palette, 1 + n palette n) |
//                        sparkle_timing i (0 free, 1..4 = 1/4 .. 1/32) | sparkle_burst T |
//                        <parameter> (named after it: speed, bars, ...)
//                        motion1_target i | motion1_shape i | motion1_from f | motion1_to f |
//                        motion1_seconds f | motion1_beats i | motion1_phase f (and motion2_...)
//   /beamled/palettes/<k>/colours i 2..7 | colour1 .. colour7 r
// Values are in real units, as the web page shows them. Preset changes from OSC play at once but,
// as edits on the web page, aren't saved until Save on the page or /beamled/save (all presets).
#pragma once
#include <Arduino.h>

void oscLoop();          // from loop(): starts once the network is up, then takes OSC in
String oscStatusText();  // for the web page
int oscUdpSocket();      // the OSC socket (UDP 8000), which Art-Net sends from too; -1 before the network
