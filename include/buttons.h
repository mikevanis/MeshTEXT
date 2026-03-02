#pragma once
#include <stdint.h>

enum ButtonEvent {
    BTN_NONE,
    BTN_SHORT,      // click
    BTN_LONG,       // long press
    BTN_DOUBLE,     // double click
    BTN_VERY_LONG   // very long press (3s+)
};

void buttonsInit();
void buttonsCheck();
ButtonEvent buttonsGetEvent();
