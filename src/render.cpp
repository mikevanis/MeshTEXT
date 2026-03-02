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

void renderInit() {
    // Enable OLED power via Vext pin (active low on Heltec V3)
    pinMode(OLED_VEXT, OUTPUT);
    digitalWrite(OLED_VEXT, LOW);
    delay(20);

    u8g2.begin();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
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

void renderPage(const Page& page) {
    u8g2.clearBuffer();

    for (uint8_t row = 0; row < PAGE_ROWS; row++) {
        for (uint8_t col = 0; col < PAGE_COLS; col++) {
            uint8_t cell = page.cells[row * PAGE_COLS + col];
            int16_t x = GRID_X_OFFSET + col * CELL_W;
            int16_t y = row * CELL_H;

            if (cell & 0x80) {
                // Block cell (0x80-0xBF: bits 0-5 are the block pattern)
                drawBlockCell(x, y, cell & 0x3F);
            } else {
                // Text cell: printable ASCII
                if (cell >= 0x20 && cell <= 0x7E) {
                    // u8g2 drawGlyph y is baseline; for 6x8 font, baseline ~= y+7
                    u8g2.drawGlyph(x, y + 7, cell);
                }
                // else: control chars / 0x00 = blank, draw nothing
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

void renderMessage(const char* line1, const char* line2) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 28, line1);
    if (line2) {
        u8g2.drawStr(0, 42, line2);
    }
    u8g2.sendBuffer();
}
