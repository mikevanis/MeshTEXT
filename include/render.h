#pragma once
#include "page.h"
#include "react.h"

// Display power states for standby mode
enum DisplayPower {
    DISPLAY_ON,
    DISPLAY_DIM,
    DISPLAY_OFF
};

// Cell dimensions in pixels
#define CELL_W 6
#define CELL_H 8
#define GRID_X_OFFSET 4  // 4px padding each side: (128 - 20*6) / 2

// Block cell sub-rectangle dimensions within 6x8 cell
#define BLOCK_SUB_W 3
#define BLOCK_SUB_H 2
// Block cell layout (2 cols x 3 rows of sub-rects, filling 6x6, bottom 2 rows empty):
//  Actually 3x2 each in a 6x8 cell:
//  Row 0 (y+0..y+1): bit0 (x+0..x+2), bit1 (x+3..x+5)
//  Row 1 (y+2..y+3): bit2 (x+0..x+2), bit3 (x+3..x+5)
//  Row 2 (y+4..y+5): bit4 (x+0..x+2), bit5 (x+3..x+5)
//  Rows y+6..y+7: unused (2px gap at bottom)

void renderInit();
void renderPage(const Page& page, const ReactTally* tally = nullptr, int16_t selectedCellIdx = -1);
void renderDirectory(const char* titles[], uint8_t count, uint8_t selected);
void renderMessage(const char* line1, const char* line2 = nullptr);

// Display power management for standby mode
void renderDim();
void renderWake();
void renderSleep();
