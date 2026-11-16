// Network access, ported from ArtSwitch (same author, MIT):
// - Ethernet with DHCP. No answer within 10 s: fall back to a fixed address (default
//   192.168.0.10/24, the pixel controller's subnet; empty = automatic 169.254.x.x from the MAC). A DHCP
//   server that shows up within 2 minutes still wins; after that the address is held.
// - http://<name>.local (mDNS), firmware updates over the network (ArduinoOTA).
// - WiFi only when the board is otherwise unreachable:
//     cable unplugged      -> join the backup WiFi network, else a hotspot named after the
//                             board (as ArtSwitch)
//     cable on fallback IP -> hotspot (the cable leads only to the pixel controller). The backup WiFi is
//                             not joined then: its subnet could clash with the cable's.
//     cable with DHCP      -> WiFi off
//   On the hotspot the board answers every DNS name with its own address.
#pragma once
#include <Arduino.h>
#include <IPAddress.h>

struct NetSettings {
    String name;          // hostname, mDNS name, hotspot name
    IPAddress fallbackIp; // 0.0.0.0 = automatic 169.254.x.x
    String wifiSsid;      // backup WiFi network, empty = hotspot only
    String wifiPass;
    String apPass;        // hotspot password, empty = open (8+ characters otherwise)
};

extern NetSettings netSettings;

void netLoad();
void netSave();
void netBegin();
void netLoop();

bool netOtaActive();        // a firmware update is running
bool netCanSend();          // an interface with an address is up
IPAddress netCurrentIp();
IPAddress netAutoFallbackIp();
String netModeText();
String netWifiText();

// WiFi scan for the settings page: request, then poll. Results are JSON [{s,r,l}].
void netRequestScan();
int netScanState();         // 0 idle, 1 busy, 2 done, 3 failed
String netScanResults();
