#pragma once
#include "page.h"
#include <Arduino.h>

// Helper: fill a page's cells from a string (space-padded to 160)
static void fillText(Page& p, const char* text) {
    memset(p.cells, 0, PAGE_CELLS);
    size_t len = strlen(text);
    for (size_t i = 0; i < PAGE_CELLS && i < len; i++) {
        p.cells[i] = (uint8_t)text[i];
    }
}

// Page 1: Welcome text page
static const Page PROGMEM testPage1 = {
    // Row 0: "   MESHTEXT v0.1   "
    // Row 1: "                    "
    // Row 2: " Peer-to-peer       "
    // Row 3: " teletext mesh      "
    // Row 4: " on ESP32 + LoRa    "
    // Row 5: "                    "
    // Row 6: " No internet.       "
    // Row 7: " No servers.        "
    {
        // Row 0
        ' ',' ',' ','M','E','S','H','T','E','X','T',' ','v','0','.','1',' ',' ',' ',' ',
        // Row 1
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 2
        ' ','P','e','e','r','-','t','o','-','p','e','e','r',' ',' ',' ',' ',' ',' ',' ',
        // Row 3
        ' ','t','e','l','e','t','e','x','t',' ','m','e','s','h',' ',' ',' ',' ',' ',' ',
        // Row 4
        ' ','o','n',' ','E','S','P','3','2','+','L','o','R','a',' ',' ',' ',' ',' ',' ',
        // Row 5
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 6
        ' ','N','o',' ','i','n','t','e','r','n','e','t','.',' ',' ',' ',' ',' ',' ',' ',
        // Row 7
        ' ','N','o',' ','s','e','r','v','e','r','s','.',' ',' ',' ',' ',' ',' ',' ',' ',
    },
    100,  // page_num
    0,    // flags
    "Welcome"
};

// Page 2: Block art demo — a simple border/frame
static const Page PROGMEM testPage2 = {
    {
        // Row 0: top border using block cells
        // Full block = all 6 bits set = 0x3F | 0x80 = 0xBF
        0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,
        0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,
        // Row 1: left + right border
        0xBF,' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',0xBF,
        // Row 2
        0xBF,' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',0xBF,
        // Row 3: center text
        0xBF,' ',' ',' ',' ','B','L','O','C','K',
        ' ','A','R','T',' ',' ',' ',' ',' ',0xBF,
        // Row 4
        0xBF,' ',' ',' ',' ','D','E','M','O',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',0xBF,
        // Row 5
        0xBF,' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',0xBF,
        // Row 6
        0xBF,' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',0xBF,
        // Row 7: bottom border
        0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,
        0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,0xBF,
    },
    101,  // page_num
    0,    // flags
    "Block Art"
};

// Page 3: Mixed content — text with block graphic accents
static const Page PROGMEM testPage3 = {
    {
        // Row 0: checkerboard pattern blocks + title
        0x95,0xAA,0x95,0xAA,' ','N','E','W','S',' ','F','L','A','S','H',0xAA,0x95,0xAA,0x95,0xAA,
        // Row 1
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 2
        ' ','L','o','R','a',' ','m','e','s','h',' ','i','s',' ',' ',' ',' ',' ',' ',' ',
        // Row 3
        ' ','n','o','w',' ','o','n','l','i','n','e','!',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 4
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 5: small block graphic (arrow-ish)
        ' ',' ',0x83,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 6
        ' ',' ',0x8F,0xBF,0xBF,' ','M','o','r','e',' ','s','o','o','n','.',' ',' ',' ',' ',
        // Row 7
        ' ',' ',0x83,' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    },
    102,  // page_num
    0,    // flags
    "News"
};

// Page 4: Title/info page
static const Page PROGMEM testPage4 = {
    {
        // Row 0
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 1
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 2
        ' ',' ',' ','H','E','L','L','O',' ','F','R','O','M',' ',' ',' ',' ',' ',' ',' ',
        // Row 3
        ' ',' ',' ','N','O','D','E',' ','A','3','F','0','B','2','1','C',' ',' ',' ',' ',
        // Row 4
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        // Row 5
        ' ',' ',' ','8','6','8',' ','M','H','z',' ','E','U',' ',' ',' ',' ',' ',' ',' ',
        // Row 6
        ' ',' ',' ','S','F','7',' ','1','2','5','k','H','z',' ',' ',' ',' ',' ',' ',' ',
        // Row 7
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    },
    103,  // page_num
    0,    // flags
    "Node Info"
};

#define TEST_PAGE_COUNT 4

static inline void getTestPage(uint8_t index, Page& out) {
    switch (index) {
        case 0: memcpy_P(&out, &testPage1, sizeof(Page)); break;
        case 1: memcpy_P(&out, &testPage2, sizeof(Page)); break;
        case 2: memcpy_P(&out, &testPage3, sizeof(Page)); break;
        case 3: memcpy_P(&out, &testPage4, sizeof(Page)); break;
    }
}

static inline const char* getTestPageTitle(uint8_t index) {
    static char buf[16];
    Page p;
    getTestPage(index, p);
    memcpy(buf, p.title, 16);
    return buf;
}
