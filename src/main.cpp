#include <Arduino.h>
#include "render.h"
#include "buttons.h"
#include "nav.h"
#include "storage.h"
#include "config.h"
#include "page.h"
#include "webportal.h"
#include "radio.h"
#include "protocol.h"
#include "mesh.h"
#include "react.h"

static void createDefaultPage() {
    Page p;
    memset(&p, 0, sizeof(p));
    p.page_num = 100;
    strlcpy(p.title, "Welcome", sizeof(p.title));

    const char* lines[] = {
        "   MESHTEXT v0.1    ",
        "                    ",
        " Peer-to-peer       ",
        " teletext mesh      ",
        " on ESP32 + LoRa    ",
        "                    ",
        " No internet.       ",
        " No servers.        ",
    };
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 20; col++) {
            p.cells[row * 20 + col] = lines[row][col];
        }
    }
    storageSavePage(p);
}

// Radio test mode
static bool radioTestMode = false;
static uint32_t lastTestSend = 0;
static uint16_t testCounter = 0;

// Stress test state
static bool stressRunning = false;
static bool stressPicking = false;  // waiting for user to pick a neighbor
static uint32_t stressNodeId = 0;
static uint8_t stressPageNum = 0;
static uint16_t stressTotal = 0;
static uint16_t stressDone = 0;
static uint16_t stressOk = 0;
static uint16_t stressFail = 0;
static uint16_t stressRetries = 0;
static bool stressWaiting = false;
static uint32_t stressStartTime = 0;

// Store neighbor IDs for picker menu
#define STRESS_MAX_PICKS 8
static uint32_t stressPickIds[STRESS_MAX_PICKS];
static uint8_t stressPickCount = 0;

void radioTestToggle() {
    radioTestMode = !radioTestMode;
    Serial.printf("Radio test mode: %s\n", radioTestMode ? "ON" : "OFF");
}

static void radioTestLoop() {
    if (!radioTestMode) return;
    if (millis() - lastTestSend < 5000) return;
    lastTestSend = millis();

    // Send a test announce packet
    AnnouncePacket pkt;
    NodeConfig& cfg = configGet();
    initHeader(pkt.hdr, PKT_ANNOUNCE, cfg.node_id, BROADCAST_ADDR);
    strlcpy(pkt.name, cfg.name, sizeof(pkt.name));
    pkt.category = cfg.category;
    pkt.page_count = testCounter++;
    pkt.first_page = 100;
    pkt.last_page = 100;

    if (radioSend((uint8_t*)&pkt, sizeof(pkt))) {
        Serial.printf("TX test #%d (pkt_id=%04X)\n", testCounter - 1, pkt.hdr.pkt_id);
    } else {
        Serial.println("TX failed");
    }
}


static void stressLoop() {
    if (!stressRunning) return;

    if (!stressWaiting) {
        // Send next request
        meshRequestPage(stressNodeId, stressPageNum);
        stressWaiting = true;
        return;
    }

    // Check for response
    Page p;
    bool timedOut;
    if (meshGetResponse(p, timedOut)) {
        uint8_t retries = meshGetRetryCount();
        stressRetries += retries;
        stressOk++;
        stressDone++;
        Serial.printf("  [%d/%d] OK%s\n", stressDone, stressTotal,
            retries > 0 ? String(" (retried " + String(retries) + "x)").c_str() : "");
        stressWaiting = false;
    } else if (timedOut) {
        stressFail++;
        stressDone++;
        Serial.printf("  [%d/%d] FAIL (timed out after retries)\n", stressDone, stressTotal);
        stressWaiting = false;
    } else {
        return; // still waiting
    }

    // Check if done
    if (stressDone >= stressTotal) {
        uint32_t elapsed = millis() - stressStartTime;
        Serial.println("--- Stress test complete ---");
        Serial.printf("  Total:   %d requests\n", stressTotal);
        Serial.printf("  OK:      %d (%.0f%%)\n", stressOk, stressTotal > 0 ? 100.0f * stressOk / stressTotal : 0);
        Serial.printf("  Failed:  %d\n", stressFail);
        Serial.printf("  Retries: %d\n", stressRetries);
        Serial.printf("  Time:    %.1fs (%.1fs avg)\n",
            elapsed / 1000.0f, stressDone > 0 ? elapsed / 1000.0f / stressDone : 0);
        stressRunning = false;
    }
}

static char serialBuf[64];
static uint8_t serialPos = 0;

static void processCommand(const char* line);

static void handleSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 0x1B) {  // ESC — clear current input
            if (serialPos > 0) {
                // Erase the line on terminal
                for (uint8_t i = 0; i < serialPos; i++) Serial.print("\b \b");
                serialPos = 0;
            }
            continue;
        }
        if (c == '\b' || c == 0x7F) {  // Backspace/DEL
            if (serialPos > 0) {
                serialPos--;
                Serial.print("\b \b");
            }
            continue;
        }
        if (c == '\r' || c == '\n') {
            Serial.println();
            if (serialPos > 0) {
                serialBuf[serialPos] = '\0';
                processCommand(serialBuf);
                serialPos = 0;
            }
            Serial.print("> ");
            continue;
        }
        if (serialPos < sizeof(serialBuf) - 1) {
            serialBuf[serialPos++] = c;
            Serial.print(c);  // echo
        }
    }
}

static void startStress() {
    stressDone = 0;
    stressOk = 0;
    stressFail = 0;
    stressRetries = 0;
    stressWaiting = false;
    stressStartTime = millis();
    stressRunning = true;
    Serial.printf("--- Stress test: %d requests for page %d from %08X ---\n",
        stressTotal, stressPageNum, stressNodeId);
}

static void processCommand(const char* input) {
    String line(input);
    line.trim();

    // Handle stress test picker
    if (stressPicking) {
        stressPicking = false;
        int pick = line.toInt();
        if (pick < 1 || pick > stressPickCount) {
            Serial.println("Cancelled.");
            return;
        }
        stressNodeId = stressPickIds[pick - 1];
        startStress();
        return;
    }

    if (line == "list") {
        PageListEntry list[MAX_PAGES];
        uint8_t count = storageListPages(list, MAX_PAGES);
        Serial.printf("%d pages:\n", count);
        for (uint8_t i = 0; i < count; i++) {
            Serial.printf("  %3d: %s\n", list[i].page_num, list[i].title);
        }
    }
    else if (line.startsWith("create ")) {
        int num = line.substring(7).toInt();
        if (num < 1 || num > 255) {
            Serial.println("Invalid page number (1-255)");
            return;
        }
        // Extract title if quoted
        char title[16] = "Untitled";
        int q1 = line.indexOf('"');
        int q2 = line.lastIndexOf('"');
        if (q1 >= 0 && q2 > q1) {
            String t = line.substring(q1 + 1, q2);
            strlcpy(title, t.c_str(), sizeof(title));
        }
        Page p;
        memset(&p, 0, sizeof(p));
        p.page_num = num;
        strlcpy(p.title, title, sizeof(p.title));
        // Fill with spaces
        memset(p.cells, ' ', PAGE_CELLS);
        if (storageSavePage(p)) {
            Serial.printf("Created page %d: %s\n", num, title);
            navRefreshDirectory();
        } else {
            Serial.println("Save failed");
        }
    }
    else if (line.startsWith("delete ")) {
        int num = line.substring(7).toInt();
        if (storageDeletePage(num)) {
            Serial.printf("Deleted page %d\n", num);
            navRefreshDirectory();
        } else {
            Serial.printf("Page %d not found\n", num);
        }
    }
    else if (line == "radiotest") {
        radioTestToggle();
    }
    else if (line.startsWith("stress")) {
        if (stressRunning) {
            stressRunning = false;
            Serial.println("Stress test aborted.");
            return;
        }

        // Parse optional args: stress [page] [count]
        uint8_t pageNum = 100;
        uint16_t count = 10;
        int sp1 = line.indexOf(' ');
        if (sp1 > 0) {
            pageNum = line.substring(sp1 + 1).toInt();
            int sp2 = line.indexOf(' ', sp1 + 1);
            if (sp2 > 0) {
                count = line.substring(sp2 + 1).toInt();
            }
        }
        if (count == 0 || count > 1000) count = 10;
        stressPageNum = pageNum;
        stressTotal = count;

        // List neighbors for selection
        Neighbor* nbrs;
        meshGetNeighbors(&nbrs);
        stressPickCount = 0;
        for (uint8_t i = 0; i < MAX_NEIGHBORS && stressPickCount < STRESS_MAX_PICKS; i++) {
            if (!nbrs[i].active) continue;
            uint32_t age = (millis() - nbrs[i].last_seen) / 1000;
            if (age > 1800) continue;
            stressPickIds[stressPickCount] = nbrs[i].node_id;
            Serial.printf("  %d) %s (%08X) rssi=%.0f\n",
                stressPickCount + 1, nbrs[i].name, nbrs[i].node_id, nbrs[i].rssi);
            stressPickCount++;
        }

        if (stressPickCount == 0) {
            Serial.println("No neighbors found.");
            return;
        }

        Serial.printf("Pick a node (1-%d): ", stressPickCount);
        stressPicking = true;
    }
    else if (line == "neighbors") {
        Neighbor* neighbors;
        uint8_t nCount = meshGetNeighbors(&neighbors);
        Serial.printf("%d neighbors:\n", nCount);
        for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
            if (!neighbors[i].active) continue;
            uint32_t age = (millis() - neighbors[i].last_seen) / 1000;
            Serial.printf("  %08X  %-15s  rssi=%.0f  %ds ago\n",
                neighbors[i].node_id, neighbors[i].name, neighbors[i].rssi, age);
        }
    }
    else {
        Serial.println("Commands: list, create, delete, radiotest, neighbors, stress");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("MeshText starting...");

    renderInit();
    renderSplash();
    delay(2000);
    renderHelpScreen();
    delay(4000);

    buttonsInit();

    if (!storageInit()) {
        renderMessage("Storage error!");
        while (1) delay(1000);
    }

    configInit();

    if (!radioInit()) {
        Serial.println("WARNING: Radio init failed, continuing without LoRa");
    }

    meshInit();
    reactInit();

    // Create default page on first boot
    PageListEntry list[1];
    if (storageListPages(list, 1) == 0) {
        Serial.println("First boot - creating default page");
        createDefaultPage();
    }

    navInit();

    NodeConfig& cfg = configGet();
    Serial.printf("Node: %s (ID: %08X)\n", cfg.name, cfg.node_id);
    Serial.println("Controls: click=scroll, double-click=select, long=back");
    Serial.println("Serial: list, create N \"title\", delete N");
    Serial.print("> ");
}

void loop() {
    buttonsCheck();
    ButtonEvent evt = buttonsGetEvent();

    if (evt != BTN_NONE) {
        const char* names[] = {"", "SHORT", "LONG", "DOUBLE", "VLONG"};
        Serial.printf("Button: %s\n", names[evt]);
    }

    // If in standby, any button wakes display (consumed by navHandleButton)
    if (evt != BTN_NONE && navIsStandby()) {
        navHandleButton(evt);
        navRender();
    }
    // Very long press toggles WiFi edit mode
    else if (evt == BTN_VERY_LONG) {
        if (portalIsActive()) {
            portalStop();
            navRefreshDirectory();
            navRender();
        } else {
            portalStart();
        }
    } else if (!portalIsActive()) {
        navHandleButton(evt);
        navRender();
    }

    portalLoop();
    meshLoop();
    navLoop();
    radioTestLoop();
    stressLoop();
    handleSerial();
}
