#pragma once
#include <Arduino.h>
#include <esp_http_server.h>

void webBegin();
void webHandle();  // from loop(): a restart asked for by the network settings
// Something the page shows but OSC doesn't carry changed (a name, the preset list, layout, ...):
// open pages reload their settings
void webChanged();
String webStatusJson();  // what the page's status line shows, pushed to it once a second
// For the HTTP server (osc.cpp), on its task: the page, a browser's redirect to it, the API
esp_err_t webPage(httpd_req_t* req);
esp_err_t webRedirect(httpd_req_t* req);
bool webApi(httpd_req_t* req, esp_err_t& result);  // false: no such /api/ address