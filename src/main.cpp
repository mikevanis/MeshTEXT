#include <Arduino.h>
#include "render.h"
#include "buttons.h"
#include "nav.h"
#include "storage.h"
#include "config.h"
#include "page.h"

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

static void processCommand(const char* input) {
    String line(input);
    line.trim();

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
    else {
        Serial.println("Commands: list, create N \"title\", delete N");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("MeshText starting...");

    renderInit();
    buttonsInit();

    if (!storageInit()) {
        renderMessage("Storage error!");
        while (1) delay(1000);
    }

    configInit();

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

    navHandleButton(evt);
    navRender();
    handleSerial();
}
