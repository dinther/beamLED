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
// WiFi waits this long without an Ethernet link, so a link coming up at boot or a cable being
// re-seated doesn't start WiFi
static const uint32_t WIFI_START_DELAY_MS = 5000;
static const uint32_t WIFI_JOIN_TIMEOUT_MS = 30000;  // then open the hotspot
static const uint32_t WIFI_RETRY_MS = 10000;         // between join attempts
// While the hotspot is up, joining is retried only when nobody is connected to it: a join
// attempt scans all channels and would drop the phone using the hotspot
static const uint32_t HOTSPOT_RETRY_MS = 30000;
static const uint32_t SCAN_TIMEOUT_MS = 15000;

static Preferences prefs;

static volatile bool linkUp = false;
static volatile bool gotIp = false;
static uint32_t linkUpAt = 0;
static volatile uint32_t linkDownAt = 0;  // boot counts as link down
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

enum WifiState { WIFI_STANDBY, WIFI_JOINING, WIFI_JOINED, WIFI_HOTSPOT };
static volatile WifiState wifiState = WIFI_STANDBY;
static volatile bool wifiGotIp = false;
static uint32_t wifiStateAt = 0;
static uint32_t lastJoinAt = 0;
static bool hotspotWithSta = false;
static DNSServer hotspotDns;

enum ScanState { SCAN_IDLE, SCAN_REQUESTED, SCAN_RUNNING, SCAN_DONE, SCAN_FAILED };
static volatile ScanState scanState = SCAN_IDLE;
static String scanResults;
static bool scanRadioOnly = false;
static uint32_t scanStartedAt = 0;

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

static String jsonEscape(const String& in) {
    String out;
    for (char c : in) {
        if (c == '"' || c == '\\') out += '\\';
        if ((uint8_t)c >= 0x20) out += c;
    }
    return out;
}

void netLoad() {
    prefs.begin("net", true);
    netSettings.name = sanitizeName(prefs.getString("name", DEFAULT_NAME));
    const IPAddress def(cfg::ETH_IP[0], cfg::ETH_IP[1], cfg::ETH_IP[2], cfg::ETH_IP[3]);
    netSettings.fallbackIp = IPAddress(prefs.getUInt("fallback", (uint32_t)def));
    netSettings.wifiSsid = prefs.getString("wssid", "");
    netSettings.wifiPass = prefs.getString("wpass", "");
    netSettings.apPass = prefs.getString("appass", "");
    prefs.end();
}

void netSave() {
    netSettings.name = sanitizeName(netSettings.name);
    if (netSettings.apPass.length() && netSettings.apPass.length() < 8) netSettings.apPass = "";
    prefs.begin("net", false);
    prefs.putString("name", netSettings.name);
    prefs.putUInt("fallback", (uint32_t)netSettings.fallbackIp);
    prefs.putString("wssid", netSettings.wifiSsid);
    prefs.putString("wpass", netSettings.wifiPass);
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
            linkDownAt = millis();
            // A DHCP lease belongs to the network the cable was in: the next cable may lead
            // elsewhere (straight to a pixel controller), so ask again, then fall back
            if (!usingFallback) gotIp = false;
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("WiFi IP address: %s\n", WiFi.localIP().toString().c_str());
            wifiGotIp = true;
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            wifiGotIp = false;
            break;
        default:
            break;
    }
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

// ---------------------------------------------------------------- WiFi

static void setWifiState(WifiState s) {
    wifiState = s;
    wifiStateAt = millis();
}

static void joinWifi() {
    lastJoinAt = millis();
    WiFi.begin(netSettings.wifiSsid.c_str(), netSettings.wifiPass.c_str());
}

// WiFi joins with the Ethernet MAC, so the router hands out the same address (and name) as on
// the cable. Set on the driver and on the IP interface, which keeps its own copy.
static void useEthernetMacForWifi() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    esp_wifi_set_mac(WIFI_IF_STA, mac);
    esp_netif_set_mac(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), mac);
}

// withSta: keep the station side so joining the backup network can be retried from the hotspot
static void startHotspot(bool withSta) {
    hotspotWithSta = withSta && netSettings.wifiSsid.length();
    WiFi.mode(hotspotWithSta ? WIFI_AP_STA : WIFI_AP);
    if (hotspotWithSta) useEthernetMacForWifi();
    const char* pass = netSettings.apPass.length() >= 8 ? netSettings.apPass.c_str() : nullptr;
    WiFi.softAP(netSettings.name.c_str(), pass);
    hotspotDns.start(53, "*", WiFi.softAPIP());
    lastJoinAt = millis();
    setWifiState(WIFI_HOTSPOT);
    Serial.printf("WiFi hotspot \"%s\" (%s), http://%s\n", netSettings.name.c_str(),
                  pass ? "password" : "open", WiFi.softAPIP().toString().c_str());
}

static void startWifi() {
    WiFi.setHostname(netSettings.name.c_str());
    WiFi.setAutoReconnect(false);  // retries are paced by updateWifi()
    if (!netSettings.wifiSsid.length()) {
        startHotspot(true);
        return;
    }
    WiFi.mode(WIFI_STA);
    useEthernetMacForWifi();
    joinWifi();
    setWifiState(WIFI_JOINING);
    Serial.printf("No Ethernet link, joining WiFi \"%s\"\n", netSettings.wifiSsid.c_str());
}

static void stopWifi() {
    if (wifiState == WIFI_STANDBY) return;
    hotspotDns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiGotIp = false;
    setWifiState(WIFI_STANDBY);
    Serial.println("WiFi off");
}

static void updateWifi() {
    if (scanRadioOnly) return;  // let a scan finish before WiFi takes over the radio
    uint32_t now = millis();

    if (linkUp) {
        // Cable in a network with DHCP (or still asking): reachable over the cable
        if (!usingFallback) { stopWifi(); return; }
        // Cable on the fallback address leads only to the pixel controller: hotspot for the web page
        if (wifiState != WIFI_HOTSPOT || hotspotWithSta) {
            stopWifi();
            startHotspot(false);
        }
        hotspotDns.processNextRequest();
        return;
    }

    // Cable unplugged: backup WiFi as in ArtSwitch
    if (dhcpProbing) {  // the fallback probe belongs to Ethernet; a re-plugged cable retries DHCP
        dhcpProbe.stop();
        dhcpProbing = false;
    }
    switch (wifiState) {
        case WIFI_STANDBY:
            if (now - linkDownAt >= WIFI_START_DELAY_MS) startWifi();
            break;
        case WIFI_JOINING:
            if (wifiGotIp) {
                setWifiState(WIFI_JOINED);
                Serial.printf("Joined WiFi \"%s\"\n", netSettings.wifiSsid.c_str());
            } else if (now - wifiStateAt >= WIFI_JOIN_TIMEOUT_MS) {
                startHotspot(true);
            } else if (now - lastJoinAt >= WIFI_RETRY_MS) {
                joinWifi();
            }
            break;
        case WIFI_JOINED:
            if (!wifiGotIp) {
                Serial.println("WiFi connection lost");
                joinWifi();
                setWifiState(WIFI_JOINING);
            }
            break;
        case WIFI_HOTSPOT:
            if (wifiGotIp) {
                hotspotDns.stop();
                WiFi.softAPdisconnect(true);
                setWifiState(WIFI_JOINED);
                Serial.printf("Joined WiFi \"%s\", hotspot closed\n", netSettings.wifiSsid.c_str());
            } else if (hotspotWithSta && WiFi.softAPgetStationNum() == 0 &&
                       now - lastJoinAt >= HOTSPOT_RETRY_MS) {
                joinWifi();
            } else {
                hotspotDns.processNextRequest();
            }
            break;
    }
}

void netRequestScan() {
    if (scanState != SCAN_REQUESTED && scanState != SCAN_RUNNING) scanState = SCAN_REQUESTED;
}

int netScanState() {
    switch (scanState) {
        case SCAN_IDLE: return 0;
        case SCAN_DONE: return 2;
        case SCAN_FAILED: return 3;
        default: return 1;
    }
}

String netScanResults() { return scanState == SCAN_DONE ? scanResults : String("[]"); }

static void updateScan() {
    if (scanState == SCAN_REQUESTED) {
        if (wifiState == WIFI_JOINING) return;  // can't scan during a join attempt; wait for it
        if (wifiState == WIFI_STANDBY) {
            WiFi.mode(WIFI_STA);
            scanRadioOnly = true;
        }
        scanStartedAt = millis();
        scanState = WiFi.scanNetworks(true) == WIFI_SCAN_FAILED ? SCAN_FAILED : SCAN_RUNNING;
        if (scanState == SCAN_RUNNING) return;
    } else if (scanState == SCAN_RUNNING) {
        // Some scans outlast the 6 s after which WiFi.scanComplete() gives up, so wait for the
        // scan-done flag with our own limit
        bool done = WiFiGenericClass::getStatusBits() & WIFI_SCAN_DONE_BIT;
        if (!done && millis() - scanStartedAt < SCAN_TIMEOUT_MS) return;
        int n = done ? WiFi.scanComplete() : WIFI_SCAN_FAILED;
        struct Net { String ssid; int rssi; bool locked; };
        std::vector<Net> nets;  // one entry per name (strongest AP), strongest first, max 20
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (!ssid.length()) continue;
            bool seen = false;
            for (Net& net : nets) {
                if (net.ssid != ssid) continue;
                net.rssi = max(net.rssi, (int)WiFi.RSSI(i));
                seen = true;
            }
            if (!seen) nets.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
        }
        std::sort(nets.begin(), nets.end(), [](const Net& a, const Net& b) { return a.rssi > b.rssi; });
        String json = "[";
        for (size_t i = 0; i < nets.size() && i < 20; i++) {
            json += String(i ? "," : "") + "{\"s\":\"" + jsonEscape(nets[i].ssid) +
                    "\",\"r\":" + nets[i].rssi + ",\"l\":" + (nets[i].locked ? "true" : "false") + "}";
        }
        scanResults = json + "]";
        WiFi.scanDelete();
        scanState = n < 0 ? SCAN_FAILED : SCAN_DONE;
    } else {
        return;
    }
    if (scanRadioOnly) {
        scanRadioOnly = false;
        if (wifiState == WIFI_STANDBY) WiFi.mode(WIFI_OFF);
    }
}

// ---------------------------------------------------------------- status

static bool onWifi() { return (!linkUp || usingFallback) && wifiState != WIFI_STANDBY; }

bool netOtaActive() { return otaActive; }

bool netCanSend() { return (linkUp && gotIp) || wifiGotIp; }

// Address the web page is reachable on right now
IPAddress netCurrentIp() {
    if (!onWifi()) return ETH.localIP();
    if (wifiState == WIFI_HOTSPOT) return WiFi.softAPIP();
    return WiFi.localIP();
}

String netModeText() {
    if (!linkUp && wifiState == WIFI_STANDBY) return "no cable";
    if (linkUp && usingFallback) return "Ethernet " + ETH.localIP().toString() + " (fallback) + hotspot";
    if (linkUp) return gotIp ? "Ethernet " + ETH.localIP().toString() + " (DHCP)" : "Ethernet, asking DHCP";
    if (wifiState == WIFI_HOTSPOT) return "hotspot";
    if (wifiState == WIFI_JOINED) return "WiFi " + WiFi.localIP().toString();
    return "joining WiFi";
}

String netWifiText() {
    const String& ssid = netSettings.wifiSsid;
    switch (wifiState) {
        case WIFI_JOINING: return "joining " + ssid + "...";
        case WIFI_JOINED: return ssid + " (" + String(WiFi.RSSI()) + " dBm)";
        case WIFI_HOTSPOT:
            return "hotspot " + netSettings.name + " at " + WiFi.softAPIP().toString() + ", " +
                   String(WiFi.softAPgetStationNum()) + " connected";
        default: return ssid.length() ? "off, backup: " + ssid : "off, backup: hotspot";
    }
}

// ---------------------------------------------------------------- services

static void startServicesOnce() {
    if (servicesStarted || !(gotIp || wifiGotIp || wifiState == WIFI_HOTSPOT)) return;
    servicesStarted = true;
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

void netBegin() {
    WiFi.onEvent(onNetworkEvent);
    WiFi.persistent(false);  // the network lives in our own settings
    WiFi.mode(WIFI_OFF);
    ETH.begin();  // pins and PHY come from the wt32-eth01 board variant
}

void netLoop() {
    checkDhcpFallback();
    if (dhcpRestartRequested) restartDhcp();
    probeForDhcpServer();
    updateWifi();
    updateScan();
    startServicesOnce();
    if (servicesStarted) ArduinoOTA.handle();
}
