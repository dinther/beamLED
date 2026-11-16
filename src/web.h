#pragma once
#include <Arduino.h>

void webBegin();
void webHandle();
// Something the page shows but OSC doesn't carry changed (a name, the preset list, layout, ...):
// open pages reload their settings
void webChanged();
String webStatusJson();  // what the page's status line shows, pushed to it once a second
