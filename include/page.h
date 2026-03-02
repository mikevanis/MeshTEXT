#pragma once
#include <stdint.h>

#define PAGE_COLS 20
#define PAGE_ROWS 8
#define PAGE_CELLS (PAGE_COLS * PAGE_ROWS)  // 160

#define PAGE_FLAG_AUTOCYCLE  0x01
#define PAGE_FLAG_POLL       0x02
#define PAGE_FLAG_VISITOR    0x04

struct Page {
    uint8_t cells[PAGE_CELLS];  // row-major, 20x8
    uint8_t page_num;           // 1-255 (0 reserved for directory)
    uint8_t flags;
    char    title[16];          // null-terminated
};
// sizeof(Page) = 178 bytes
