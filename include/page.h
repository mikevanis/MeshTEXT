#pragma once
#include <stdint.h>

#define PAGE_COLS 20
#define PAGE_ROWS 8
#define PAGE_CELLS (PAGE_COLS * PAGE_ROWS)  // 160

#define PAGE_FLAG_AUTOCYCLE  0x01
#define PAGE_FLAG_POLL       0x02
#define PAGE_FLAG_VISITOR    0x04

// Dynamic cell types (0xF0-0xFF range, above block cells 0x80-0xBF)
#define CELL_VISITOR_COUNT   0xF0  // renders as visitor count digits
#define CELL_VOTE_A_BTN      0xF1  // vote A button (selectable)
#define CELL_VOTE_B_BTN      0xF2  // vote B button (selectable)
#define CELL_VOTE_A_COUNT    0xF3  // renders as vote A count digits
#define CELL_VOTE_B_COUNT    0xF4  // renders as vote B count digits
#define CELL_LINK            0xF5  // hyperlink: next cell = target page number

#define IS_DYNAMIC_CELL(c)   ((c) >= 0xF0 && (c) <= 0xF5)

struct Page {
    uint8_t cells[PAGE_CELLS];  // row-major, 20x8
    uint8_t page_num;           // 1-255 (0 reserved for directory)
    uint8_t flags;
    char    title[16];          // null-terminated
};
// sizeof(Page) = 178 bytes
