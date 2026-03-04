#include "webportal.h"
#include "storage.h"
#include "config.h"
#include "render.h"
#include "nav.h"
#include "react.h"
#include "page.h"
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

static AsyncWebServer server(80);
static DNSServer dns;
static bool active = false;
static char ssid[20];

static void setupRoutes() {
    // Serve editor from LittleFS
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(LittleFS, "/editor.html", "text/html");
    });

    // Captive portal redirects
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/");
    });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/");
    });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    // GET /api/config
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
        NodeConfig& cfg = configGet();
        JsonDocument doc;
        doc["name"] = cfg.name;
        doc["category"] = cfg.category;
        doc["node_id"] = String(cfg.node_id, HEX);
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // PUT /api/config
    server.on("/api/config", HTTP_PUT, [](AsyncWebServerRequest* req) {},
        NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) { req->send(400, "text/plain", "Bad JSON"); return; }
            NodeConfig& cfg = configGet();
            if (doc["name"].is<const char*>())
                strlcpy(cfg.name, doc["name"] | cfg.name, sizeof(cfg.name));
            if (doc["category"].is<int>())
                cfg.category = doc["category"] | cfg.category;
            configSave();
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // GET /api/pages
    server.on("/api/pages", HTTP_GET, [](AsyncWebServerRequest* req) {
        PageListEntry list[MAX_PAGES];
        uint8_t count = storageListPages(list, MAX_PAGES);
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (uint8_t i = 0; i < count; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["page_num"] = list[i].page_num;
            obj["title"] = list[i].title;
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // GET /api/page/:n
    server.on("^\\/api\\/page\\/(\\d+)$", HTTP_GET, [](AsyncWebServerRequest* req) {
        int num = req->pathArg(0).toInt();
        Page p;
        if (!storageLoadPage(num, p)) {
            req->send(404, "text/plain", "Not found");
            return;
        }
        JsonDocument doc;
        doc["page_num"] = p.page_num;
        doc["flags"] = p.flags;
        doc["title"] = p.title;
        JsonArray cells = doc["cells"].to<JsonArray>();
        for (int i = 0; i < PAGE_CELLS; i++) {
            cells.add(p.cells[i]);
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // PUT /api/page/:n
    server.on("^\\/api\\/page\\/(\\d+)$", HTTP_PUT, [](AsyncWebServerRequest* req) {},
        NULL,
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            int num = req->pathArg(0).toInt();
            if (num < 1 || num > 255) { req->send(400, "text/plain", "Invalid page"); return; }

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (err) { req->send(400, "text/plain", "Bad JSON"); return; }

            Page p;
            memset(&p, 0, sizeof(p));
            p.page_num = num;
            p.flags = doc["flags"] | 0;
            strlcpy(p.title, doc["title"] | "Untitled", sizeof(p.title));

            JsonArray cells = doc["cells"];
            if (cells && cells.size() == PAGE_CELLS) {
                for (int i = 0; i < PAGE_CELLS; i++) {
                    p.cells[i] = (uint8_t)(cells[i].as<int>());
                }
            }

            if (storageSavePage(p)) {
                req->send(200, "application/json", "{\"ok\":true}");
            } else {
                req->send(500, "text/plain", "Save failed");
            }
        }
    );

    // DELETE /api/page/:n
    server.on("^\\/api\\/page\\/(\\d+)$", HTTP_DELETE, [](AsyncWebServerRequest* req) {
        int num = req->pathArg(0).toInt();
        if (storageDeletePage(num)) {
            req->send(200, "application/json", "{\"ok\":true}");
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });

    // GET /api/react/:n — get react tally for a page
    server.on("^\\/api\\/react\\/(\\d+)$", HTTP_GET, [](AsyncWebServerRequest* req) {
        int num = req->pathArg(0).toInt();
        ReactTally tally;
        if (!reactGetTally(num, tally)) {
            req->send(200, "application/json", "{\"visits\":0,\"votes_a\":0,\"votes_b\":0}");
            return;
        }
        JsonDocument doc;
        doc["visits"] = tally.visits;
        doc["votes_a"] = tally.votes_a;
        doc["votes_b"] = tally.votes_b;
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // Catch-all for captive portal
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("/");
    });
}

void portalStart() {
    if (active) return;

    NodeConfig& cfg = configGet();
    snprintf(ssid, sizeof(ssid), "MeshTxt-%04X", (uint16_t)(cfg.node_id & 0xFFFF));

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid);
    delay(100);

    dns.start(53, "*", WiFi.softAPIP());
    setupRoutes();
    server.begin();

    active = true;
    Serial.printf("WiFi AP started: %s (%s)\n", ssid, WiFi.softAPIP().toString().c_str());
    renderMessage("EDIT MODE", ssid);
}

void portalStop() {
    if (!active) return;

    server.end();
    dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    active = false;
    Serial.println("WiFi AP stopped");
}

void portalLoop() {
    if (active) {
        dns.processNextRequest();
    }
}

bool portalIsActive() {
    return active;
}
