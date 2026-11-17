#include "net.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ETH.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include <algorithm>
#include <vector>

#include "config.h"

NetSettings netSettings;

static const char* DEFAULT_NAME = "beamled";
static const uint32_t DHCP_TIMEOUT_MS = 10000;
static const uint32_t DHCP_PROBE_INTERVAL_MS = 15000;
// Once someone may have configured against the fallback address, it must not move
static const uint32_t DHCP_GRACE_MS = 120000;
// Config mode (button held at power-on): the hotspot and the web server only, no Ethernet.
// Otherwise WiFi never starts at all: on this board the hotspot jammed while Ethernet ran.
static bool configMode = false;

static Preferences prefs;

static volatile bool linkUp = false;
static volatile bool gotIp = false;
static uint32_t linkUpAt = 0;
static bool usingFallback = false;
static bool servicesStarted = false;
static volatile bool otaActive = false;
static volatile bool dhcpRestartRequested = false;

// During the grace window on the fallback address a DHCPDISCOVER is sent now and then; an offer
// means a server has appeared (e.g. the router booted after us) and we switch to DHCP.
static WiFiUDP dhcpProbe;
static bool dhcpProbing = false;
static uint32_t dhcpProbeXid = 0;
static uint32_t fallbackSince = 0;
static uint32_t lastDhcpProbeAt = 0;

static bool hotspotUp = false;
static DNSServer hotspotDns;

// ---------------------------------------------------------------- settings

static String sanitizeName(const String& in) {
    String out;
    for (char c : in) {
        c = tolower(c);
        if (isalnum(c) || c == '-') out += c;
        if (out.length() >= 31) break;
    }
    while (out.startsWith("-")) out.remove(0, 1);
    while (out.endsWith("-")) out.remove(out.length() - 1);
    return out.length() ? out : String(DEFAULT_NAME);
}

void netLoad() {
    prefs.begin("net", true);
    netSettings.name = sanitizeName(prefs.getString("name", DEFAULT_NAME));
    const IPAddress def(cfg::ETH_IP[0], cfg::ETH_IP[1], cfg::ETH_IP[2], cfg::ETH_IP[3]);
    netSettings.fallbackIp = IPAddress(prefs.getUInt("fallback", (uint32_t)def));
    netSettings.apPass = prefs.getString("appass", "");
    prefs.end();
}

void netSave() {
    netSettings.name = sanitizeName(netSettings.name);
    if (netSettings.apPass.length() && netSettings.apPass.length() < 8) netSettings.apPass = "";
    prefs.begin("net", false);
    prefs.putString("name", netSettings.name);
    prefs.putUInt("fallback", (uint32_t)netSettings.fallbackIp);
    prefs.putString("appass", netSettings.apPass);
    prefs.end();
}

// Link-local address from the MAC, so a PC that self-assigned 169.254.x.x is on the same subnet.
// 169.254.0.x and .255.x are reserved.
IPAddress netAutoFallbackIp() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    return IPAddress(169, 254, 1 + mac[4] % 254, mac[5]);
}

static IPAddress fallbackIp() {
    return (uint32_t)netSettings.fallbackIp ? netSettings.fallbackIp : netAutoFallbackIp();
}

// Classful netmask: 2.x / 10.x get /8, 169.254.x gets /16, 192.168.x gets /24
static IPAddress netmaskFor(IPAddress ip) {
    if (ip[0] < 128) return IPAddress(255, 0, 0, 0);
    if (ip[0] < 192) return IPAddress(255, 255, 0, 0);
    return IPAddress(255, 255, 255, 0);
}

// ---------------------------------------------------------------- Ethernet

static void onNetworkEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ETH.setHostname(netSettings.name.c_str());
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet link up");
            linkUp = true;
            linkUpAt = millis();
            if (usingFallback) dhcpRestartRequested = true;  // may now be in a network with DHCP
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.printf("IP address: %s (DHCP)\n", ETH.localIP().toString().c_str());
            gotIp = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet link down");
            linkUp = false;
            // A DHCP lease belongs to the network the cable was in: the next cable may lead
            // elsewhere (straight to a pixel controller), so ask again, then fall back
            if (!usingFallback) gotIp = false;
            break;
        default:
            break;
    }
}

// Hotspot clients with their MAC and address: what to look for when a phone can't reach the page
static void onHotspotEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    const uint8_t* m = nullptr;
    const char* what = nullptr;
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            m = info.wifi_ap_staconnected.mac;
            what = "joined";
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            m = info.wifi_ap_stadisconnected.mac;
            what = "left";
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.printf("Hotspot: client got %s, board is %s\n",
                          IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str(),
                          WiFi.softAPIP().toString().c_str());
            return;
        default:
            return;
    }
    Serial.printf("Hotspot: %02x:%02x:%02x:%02x:%02x:%02x %s (%d now)\n", m[0], m[1], m[2], m[3], m[4], m[5],
                  what, WiFi.softAPgetStationNum());
}

static void checkDhcpFallback() {
    if (gotIp || usingFallback || !linkUp) return;
    if (millis() - linkUpAt < DHCP_TIMEOUT_MS) return;
    IPAddress ip = fallbackIp();
    ETH.config(ip, IPAddress(0, 0, 0, 0), netmaskFor(ip));
    usingFallback = true;
    gotIp = true;
    dhcpProbe.begin(68);
    dhcpProbing = true;
    fallbackSince = lastDhcpProbeAt = millis();
    Serial.printf("No DHCP answer, using fallback IP %s/%s\n", ip.toString().c_str(),
                  netmaskFor(ip).toString().c_str());
}

static void restartDhcp() {
    dhcpRestartRequested = false;
    if (!usingFallback) return;
    dhcpProbe.stop();
    dhcpProbing = false;
    ETH.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0));
    usingFallback = false;
    gotIp = false;
    linkUpAt = millis();  // fall back again if this attempt gets no lease either
    Serial.println("Retrying DHCP");
}

static void sendDhcpDiscover() {
    uint8_t d[300] = {0};
    d[0] = 1;  // BOOTREQUEST
    d[1] = 1;  // Ethernet
    d[2] = 6;  // MAC length
    dhcpProbeXid = esp_random();
    memcpy(&d[4], &dhcpProbeXid, 4);
    d[10] = 0x80;  // ask for a broadcast reply, we have no lease
    esp_read_mac(&d[28], ESP_MAC_ETH);
    const uint8_t options[] = {0x63, 0x82, 0x53, 0x63, 53, 1, 1, 255};  // cookie, DISCOVER, end
    memcpy(&d[236], options, sizeof(options));
    dhcpProbe.beginPacket(IPAddress(255, 255, 255, 255), 67);
    dhcpProbe.write(d, sizeof(d));
    dhcpProbe.endPacket();
}

static void probeForDhcpServer() {
    if (!dhcpProbing) return;
    if (millis() - fallbackSince >= DHCP_GRACE_MS) {
        dhcpProbe.stop();
        dhcpProbing = false;
        Serial.println("No DHCP server within grace window, holding fallback address");
        return;
    }
    int len = dhcpProbe.parsePacket();
    if (len > 0) {
        uint8_t d[600];
        len = dhcpProbe.read(d, min(len, (int)sizeof(d)));
        dhcpProbe.flush();
        if (len >= 244 && d[0] == 2 && memcmp(&d[4], &dhcpProbeXid, 4) == 0) {
            for (int i = 240; i + 2 < len && d[i] != 255;) {  // option 53 = OFFER (2)
                if (d[i] == 0) { i++; continue; }
                if (d[i] == 53 && d[i + 2] == 2) {
                    Serial.println("DHCP server found");
                    restartDhcp();
                    return;
                }
                i += 2 + d[i + 1];
            }
        }
    }
    if (millis() - lastDhcpProbeAt >= DHCP_PROBE_INTERVAL_MS) {
        lastDhcpProbeAt = millis();
        sendDhcpDiscover();
    }
}

// ---------------------------------------------------------------- WiFi (config mode only)

static void startHotspot() {
    WiFi.mode(WIFI_AP);
    const char* pass = netSettings.apPass.length() >= 8 ? netSettings.apPass.c_str() : nullptr;
    WiFi.softAP(netSettings.name.c_str(), pass);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);  // as WLED ran this board: at full power the link was poor
    hotspotDns.start(53, "*", WiFi.softAPIP());
    hotspotUp = true;
    Serial.printf("WiFi hotspot \"%s\" (%s), http://%s\n", netSettings.name.c_str(),
                  pass ? "password" : "open", WiFi.softAPIP().toString().c_str());
}

// ---------------------------------------------------------------- status

bool netOtaActive() { return otaActive; }

bool netCanSend() { return linkUp && gotIp; }

// Somewhere to serve the web page: an address on the cable, or the config-mode hotspot
bool netServing() { return netCanSend() || hotspotUp; }

// Art-Net only goes out over the cable: outside config mode there is no WiFi at all
bool netCanSendTo(IPAddress) { return !configMode && linkUp && gotIp; }

// Address the web page is reachable on right now
IPAddress netCurrentIp() { return configMode ? WiFi.softAPIP() : ETH.localIP(); }

String netModeText() {
    if (configMode) return "config mode: hotspot only, no Art-Net (restart to run)";
    if (!linkUp) return "no cable";
    if (usingFallback) return "Ethernet " + ETH.localIP().toString() + " (fallback)";
    return gotIp ? "Ethernet " + ETH.localIP().toString() + " (DHCP)" : "Ethernet, asking DHCP";
}

String netWifiText() {
    if (!configMode) return "off (hold the button at power-on for the hotspot)";
    return "hotspot " + netSettings.name + " at " + WiFi.softAPIP().toString() + ", " +
           String(WiFi.softAPgetStationNum()) + " connected";
}

// ---------------------------------------------------------------- services

static void startServicesOnce() {
    if (servicesStarted || !(gotIp || hotspotUp)) return;
    servicesStarted = true;
    if (configMode) {  // the web server only (osc.cpp starts it)
        Serial.printf("Config mode: web interface on hotspot \"%s\", http://%s\n", netSettings.name.c_str(),
                      WiFi.softAPIP().toString().c_str());
        return;
    }
    if (MDNS.begin(netSettings.name.c_str())) MDNS.addService("http", "tcp", 80);
    ArduinoOTA.setHostname(netSettings.name.c_str());
    ArduinoOTA.setMdnsEnabled(false);  // mDNS already started above
    ArduinoOTA.onStart([]() {
        otaActive = true;
        Serial.println("OTA update starting");
    });
    ArduinoOTA.onEnd([]() { Serial.println("OTA update done, restarting"); });
    ArduinoOTA.onError([](ota_error_t e) {
        otaActive = false;
        Serial.printf("OTA update failed (%u)\n", e);
    });
    ArduinoOTA.begin();
    Serial.printf("Web interface: http://%s.local  or  http://%s\n", netSettings.name.c_str(),
                  netCurrentIp().toString().c_str());
}

void netBegin(bool config) {
    configMode = config;
    WiFi.onEvent(onNetworkEvent);
    WiFi.onEvent(onHotspotEvent);
    WiFi.persistent(false);  // the network lives in our own settings
    WiFi.mode(WIFI_OFF);
    if (configMode) {
        Serial.println("Config mode (button held at power-on): hotspot and web page only");
        startHotspot();
        return;
    }
    ETH.begin();  // pins and PHY come from the wt32-eth01 board variant
}

bool netConfigMode() { return configMode; }

void netLoop() {
    if (configMode) {
        hotspotDns.processNextRequest();
        startServicesOnce();
        return;
    }
    checkDhcpFallback();
    if (dhcpRestartRequested) restartDhcp();
    probeForDhcpServer();
    startServicesOnce();
    if (servicesStarted) ArduinoOTA.handle();
}
