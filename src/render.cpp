#include "render.h"
#include "pins.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// SSD1306 128x64 Software I2C on Heltec V3
// HW I2C doesn't work — V3 uses GPIO17/18 which aren't on a HW I2C bus
static U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
    U8G2_R0,
    /* clock=*/ OLED_SCL,
    /* data=*/  OLED_SDA,
    /* reset=*/ OLED_RST
);

// Battery voltage reading
static float batteryVoltage = 0;
static uint32_t lastBatteryRead = 0;
#define BATTERY_READ_INTERVAL 10000  // 10 seconds

static void batteryInit() {
    pinMode(ADC_CTRL_PIN, OUTPUT);
    digitalWrite(ADC_CTRL_PIN, LOW);   // disable measurement circuit
    analogSetAttenuation(ADC_11db);
}

static float batteryRead() {
    digitalWrite(ADC_CTRL_PIN, HIGH);  // enable measurement (V3.2: HIGH to enable)
    delay(2);
    int raw = analogRead(VBAT_PIN);
    digitalWrite(ADC_CTRL_PIN, LOW);   // disable measurement
    return (raw / 4095.0f) * 3.3f * 5.6f;  // voltage divider correction
}

// Returns 0-4 bar level from voltage
static uint8_t batteryLevel(float v) {
    if (v >= 4.0f) return 4;
    if (v >= 3.8f) return 3;
    if (v >= 3.6f) return 2;
    if (v >= 3.4f) return 1;
    return 0;
}

// Draw battery icon at top-right of screen (14x8 px)
static void drawBattery() {
    uint32_t now = millis();
    if (now - lastBatteryRead > BATTERY_READ_INTERVAL || lastBatteryRead == 0) {
        batteryVoltage = batteryRead();
        lastBatteryRead = now;
        Serial.printf("Battery: %.2fV\n", batteryVoltage);
    }

    uint8_t level = batteryLevel(batteryVoltage);

    // Percentage text to the left of icon
    int pct = (int)((batteryVoltage - 3.0f) / 1.2f * 100.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    char buf[5];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    u8g2.setFont(u8g2_font_5x7_tr);
    uint8_t tw = strlen(buf) * 5;
    int16_t x = 127 - 14 - tw - 2;  // text position
    u8g2.drawStr(x, 7, buf);
    u8g2.setFont(u8g2_font_6x10_tr);

    // Battery outline: 12x7 body + 2x3 nub
    int16_t bx = 127 - 14;  // right-aligned
    int16_t by = 1;
    u8g2.drawFrame(bx, by, 12, 7);       // body
    u8g2.drawBox(bx + 12, by + 2, 2, 3); // positive terminal nub

    // Fill bars (each bar is 2px wide with 1px gap)
    for (uint8_t i = 0; i < level; i++) {
        u8g2.drawBox(bx + 1 + i * 3, by + 1, 2, 5);
    }
}

void renderInit() {
    // Enable OLED power via Vext pin (active low on Heltec V3)
    pinMode(OLED_VEXT, OUTPUT);
    digitalWrite(OLED_VEXT, LOW);
    delay(20);

    u8g2.begin();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.clearBuffer();
    u8g2.sendBuffer();

    batteryInit();
}

static void drawBlockCell(int16_t x, int16_t y, uint8_t cell) {
    // Bits 0-5 map to 2x3 grid of sub-rectangles
    // Each sub-rect is 3px wide x 2px tall (fills 6x6 of the 6x8 cell)
    for (uint8_t bit = 0; bit < 6; bit++) {
        if (cell & (1 << bit)) {
            int16_t col = bit & 1;       // 0 or 1
            int16_t row = bit >> 1;       // 0, 1, or 2
            int16_t rx = x + col * BLOCK_SUB_W;
            int16_t ry = y + row * BLOCK_SUB_H;
            u8g2.drawBox(rx, ry, BLOCK_SUB_W, BLOCK_SUB_H);
        }
    }
}

// Draw a number starting at cell position, flowing right. Returns cells consumed.
static uint8_t drawNumber(int16_t x, int16_t y, uint16_t num) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", num);
    uint8_t len = strlen(buf);
    for (uint8_t i = 0; i < len; i++) {
        u8g2.drawGlyph(x + i * CELL_W, y + 7, buf[i]);
    }
    return len;
}

// Draw a vote button
// Unselected: letter with underline (visible but passive)
// Selected: fully inverted (white box, black letter = "this is active")
static void drawButton(int16_t x, int16_t y, char label, bool selected) {
    if (selected) {
        u8g2.drawBox(x, y, CELL_W, CELL_H);
        u8g2.setDrawColor(0);
        u8g2.drawGlyph(x, y + 7, label);
        u8g2.setDrawColor(1);
    } else {
        u8g2.drawGlyph(x, y + 7, label);
        u8g2.drawHLine(x, y + CELL_H - 1, CELL_W);
    }
}

// Draw a link cell: arrow + page number, flowing right. Returns cells consumed.
static uint8_t drawLink(int16_t x, int16_t y, uint8_t targetPage, bool selected) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%d", targetPage);
    uint8_t numLen = strlen(buf);
    uint8_t totalCells = 1 + numLen;  // arrow + digits

    if (selected) {
        // Inverted background across all cells
        u8g2.drawBox(x, y, totalCells * CELL_W, CELL_H);
        u8g2.setDrawColor(0);
        u8g2.drawGlyph(x, y + 7, '>');  // arrow character
        for (uint8_t i = 0; i < numLen; i++) {
            u8g2.drawGlyph(x + (1 + i) * CELL_W, y + 7, buf[i]);
        }
        u8g2.setDrawColor(1);
    } else {
        u8g2.drawGlyph(x, y + 7, '>');
        for (uint8_t i = 0; i < numLen; i++) {
            u8g2.drawGlyph(x + (1 + i) * CELL_W, y + 7, buf[i]);
        }
        // Underline across all cells
        u8g2.drawHLine(x, y + CELL_H - 1, totalCells * CELL_W);
    }
    return totalCells;
}

void renderPage(const Page& page, const ReactTally* tally, int16_t selectedCellIdx) {
    u8g2.clearBuffer();

    // First pass: find dynamic cells and figure out skip positions
    bool skip[PAGE_CELLS] = {};

    if (tally) {
        for (uint8_t i = 0; i < PAGE_CELLS; i++) {
            uint8_t cell = page.cells[i];
            uint16_t val = 0;
            if (cell == CELL_VISITOR_COUNT) val = tally->visits;
            else if (cell == CELL_VOTE_A_COUNT) val = tally->votes_a;
            else if (cell == CELL_VOTE_B_COUNT) val = tally->votes_b;
            else continue;

            char buf[6];
            uint8_t len = snprintf(buf, sizeof(buf), "%d", val);
            uint8_t row = i / PAGE_COLS;
            for (uint8_t d = 1; d < len && (i + d) / PAGE_COLS == row; d++) {
                skip[i + d] = true;
            }
        }
    }

    // Mark link target cells as skip (cell after CELL_LINK is page number, not rendered)
    for (uint8_t i = 0; i < PAGE_CELLS; i++) {
        if (page.cells[i] == CELL_LINK && i + 1 < PAGE_CELLS) {
            // Skip the target byte cell
            skip[i + 1] = true;
            // Also skip cells used by the rendered digits
            uint8_t target = page.cells[i + 1];
            char buf[6];
            uint8_t numLen = snprintf(buf, sizeof(buf), "%d", target);
            uint8_t row = i / PAGE_COLS;
            // Skip: arrow(1) + digits(numLen) - 1 for the link cell itself = numLen cells after link+target
            for (uint8_t d = 2; d < 1 + numLen && (i + d) / PAGE_COLS == row; d++) {
                skip[i + d] = true;
            }
        }
    }

    // Render
    for (uint8_t row = 0; row < PAGE_ROWS; row++) {
        for (uint8_t col = 0; col < PAGE_COLS; col++) {
            uint8_t idx = row * PAGE_COLS + col;
            if (skip[idx]) continue;

            uint8_t cell = page.cells[idx];
            int16_t x = GRID_X_OFFSET + col * CELL_W;
            int16_t y = row * CELL_H;

            if (cell == CELL_LINK && idx + 1 < PAGE_CELLS) {
                uint8_t target = page.cells[idx + 1];
                bool selected = (selectedCellIdx == (int16_t)idx);
                drawLink(x, y, target, selected);
            } else if (IS_DYNAMIC_CELL(cell) && tally) {
                switch (cell) {
                    case CELL_VISITOR_COUNT:
                        drawNumber(x, y, tally->visits);
                        break;
                    case CELL_VOTE_A_BTN:
                        drawButton(x, y, 'A', selectedCellIdx == (int16_t)idx);
                        break;
                    case CELL_VOTE_B_BTN:
                        drawButton(x, y, 'B', selectedCellIdx == (int16_t)idx);
                        break;
                    case CELL_VOTE_A_COUNT:
                        drawNumber(x, y, tally->votes_a);
                        break;
                    case CELL_VOTE_B_COUNT:
                        drawNumber(x, y, tally->votes_b);
                        break;
                }
            } else if (cell >= 0x80 && cell <= 0xBF) {
                drawBlockCell(x, y, cell & 0x3F);
            } else if (cell >= 0x20 && cell <= 0x7E) {
                u8g2.drawGlyph(x, y + 7, cell);
            }
        }
    }

    u8g2.sendBuffer();
}

void renderDirectory(const char* titles[], uint8_t count, uint8_t selected) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    // Title bar
    u8g2.drawStr(0, 9, "MeshText");
    drawBattery();
    u8g2.drawHLine(0, 11, 128);

    // Show up to 5 entries starting from a window around selected
    uint8_t maxVisible = 5;
    uint8_t startIdx = 0;
    if (selected >= maxVisible) {
        startIdx = selected - maxVisible + 1;
    }

    for (uint8_t i = 0; i < maxVisible && (startIdx + i) < count; i++) {
        uint8_t idx = startIdx + i;
        int16_t yPos = 23 + i * 10;

        if (idx == selected) {
            u8g2.drawBox(0, yPos - 8, 128, 10);
            u8g2.setDrawColor(0);
        }
        u8g2.drawStr(4, yPos, titles[idx]);
        if (idx == selected) {
            u8g2.setDrawColor(1);
        }
    }

    u8g2.sendBuffer();
}

void renderSplash() {
    // Block-cell art splash: "MESH" rows 0-2, "TEXT" rows 4-6, "v0.1" row 7
    // Each letter is 3 cells wide × 3 cells tall, with 1-cell gaps, starting at col 3
    // Block cell = 0x80 | bitmask, where bits 0-5 map to 2×3 sub-pixel grid

    // Letter definitions: [3 rows][3 cols]
    static const uint8_t L_M[3][3] = {
        {0xBF, 0xBC, 0xBF},  // Top: full|bottom-fill|full (crossbar effect)
        {0xBF, 0x80, 0xBF},  // Mid: pillars
        {0x8F, 0x80, 0x8F},  // Bot: pillars (bottom sub-row empty for spacing)
    };
    static const uint8_t L_E[3][3] = {
        {0xBF, 0x8F, 0x8F},  // Top: full bar
        {0xBF, 0x8F, 0x80},  // Mid: half bar (crossbar)
        {0x8F, 0x8F, 0x8F},  // Bot: full bar
    };
    static const uint8_t L_S[3][3] = {
        {0xBF, 0x8F, 0x8F},  // Top: full bar
        {0x8F, 0x8F, 0xBF},  // Mid: shifts right
        {0x8F, 0x8F, 0x8F},  // Bot: full bar
    };
    static const uint8_t L_H[3][3] = {
        {0xBF, 0x80, 0xBF},  // Top: pillars
        {0xBF, 0x8F, 0xBF},  // Mid: crossbar
        {0x8F, 0x80, 0x8F},  // Bot: pillars
    };
    static const uint8_t L_T[3][3] = {
        {0x8F, 0xBF, 0x8F},  // Top: full bar
        {0x80, 0xBF, 0x80},  // Mid: center pillar
        {0x80, 0x8F, 0x80},  // Bot: center pillar
    };
    static const uint8_t L_X[3][3] = {
        {0xAF, 0xB0, 0x9F},  // Top: diagonals in
        {0xA0, 0xBF, 0x90},  // Mid: center cross
        {0x8F, 0x80, 0x8F},  // Bot: diagonals out
    };

    // Letters for each word, with column offsets
    // MESH at row 0, TEXT at row 4
    // Each letter starts at col: 3, 7, 11, 15
    struct LetterPlacement {
        const uint8_t (*data)[3];
        uint8_t col;
        uint8_t row;
    };
    static const LetterPlacement letters[] = {
        {L_M,  3, 0}, {L_E,  7, 0}, {L_S, 11, 0}, {L_H, 15, 0},  // MESH
        {L_T,  3, 4}, {L_E,  7, 4}, {L_X, 11, 4}, {L_T, 15, 4},  // TEXT
    };

    Page splash;
    memset(&splash, ' ', sizeof(splash.cells));
    splash.page_num = 0;
    splash.flags = 0;
    splash.title[0] = '\0';

    // Place block letters
    for (const auto& lp : letters) {
        for (uint8_t r = 0; r < 3; r++) {
            for (uint8_t c = 0; c < 3; c++) {
                splash.cells[(lp.row + r) * PAGE_COLS + lp.col + c] = lp.data[r][c];
            }
        }
    }

    // Row 7: version string centered
    // MESHTEXT_VERSION comes from env var; empty string means local/dev build
    const char* ver = (sizeof(MESHTEXT_VERSION) > 1) ? MESHTEXT_VERSION : "dev";
    uint8_t vlen = strlen(ver);
    uint8_t voff = (PAGE_COLS - vlen) / 2;
    for (uint8_t i = 0; ver[i] && (voff + i) < PAGE_COLS; i++) {
        splash.cells[7 * PAGE_COLS + voff + i] = ver[i];
    }

    renderPage(splash);
}

void renderHelpScreen() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);

    u8g2.drawStr(0,  9, "Controls:");
    u8g2.drawHLine(0, 11, 128);
    u8g2.drawStr(0, 23, "Click      Scroll");
    u8g2.drawStr(0, 33, "Dbl-click  Select");
    u8g2.drawStr(0, 43, "Hold       Back");
    u8g2.drawStr(0, 53, "Hold 3s    Edit mode");
    u8g2.sendBuffer();
}

void renderDim() {
    u8g2.setContrast(0);
}

void renderSleep() {
    u8g2.setPowerSave(1);
}

void renderWake() {
    u8g2.setPowerSave(0);
    u8g2.setContrast(255);
}

void renderMessage(const char* line1, const char* line2) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 28, line1);
    if (line2) {
        u8g2.drawStr(0, 42, line2);
    }
    u8g2.sendBuffer();
}
