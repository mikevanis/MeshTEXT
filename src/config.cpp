#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <esp_mac.h>

static NodeConfig cfg;

static uint32_t readNodeIdFromMAC() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // Last 4 bytes as node_id
    return ((uint32_t)mac[2] << 24) |
           ((uint32_t)mac[3] << 16) |
           ((uint32_t)mac[4] << 8)  |
           (uint32_t)mac[5];
}

bool configInit() {
    cfg.node_id = readNodeIdFromMAC();

    File f = LittleFS.open("/config.json", "r");
    if (f) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (!err) {
            strlcpy(cfg.name, doc["name"] | "MeshNode", sizeof(cfg.name));
            cfg.category = doc["category"] | 0;
            Serial.printf("Config loaded: %s (cat %d)\n", cfg.name, cfg.category);
            return true;
        }
    }

    // First boot defaults
    snprintf(cfg.name, sizeof(cfg.name), "Node-%04X", (uint16_t)(cfg.node_id & 0xFFFF));
    cfg.category = 0;
    Serial.printf("Config defaults: %s\n", cfg.name);
    return configSave();
}

NodeConfig& configGet() {
    return cfg;
}

bool configSave() {
    JsonDocument doc;
    doc["name"] = cfg.name;
    doc["category"] = cfg.category;

    File f = LittleFS.open("/config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

uint32_t configGetNodeId() {
    return cfg.node_id;
}
