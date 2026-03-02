#include "nav.h"
#include "render.h"
#include "testpages.h"

static NavState state = NAV_DIRECTORY;
static uint8_t  dirIndex = 0;
static Page     currentPage;
static bool     needsRedraw = true;

// Directory titles (built from test pages)
static const char* dirTitles[TEST_PAGE_COUNT];
static char titleBufs[TEST_PAGE_COUNT][16];

void navInit() {
    // Cache title strings from PROGMEM test pages
    for (uint8_t i = 0; i < TEST_PAGE_COUNT; i++) {
        Page p;
        getTestPage(i, p);
        memcpy(titleBufs[i], p.title, 16);
        dirTitles[i] = titleBufs[i];
    }

    state = NAV_DIRECTORY;
    dirIndex = 0;
    needsRedraw = true;
}

void navHandleButton(ButtonEvent evt) {
    if (evt == BTN_NONE) return;

    switch (state) {
        case NAV_DIRECTORY:
            if (evt == BTN_SHORT) {
                // Scroll down
                dirIndex = (dirIndex + 1) % TEST_PAGE_COUNT;
                needsRedraw = true;
            }
            else if (evt == BTN_DOUBLE) {
                // Select page
                getTestPage(dirIndex, currentPage);
                state = NAV_PAGE_VIEW;
                needsRedraw = true;
            }
            break;

        case NAV_PAGE_VIEW:
            if (evt == BTN_LONG) {
                // Back to directory
                state = NAV_DIRECTORY;
                needsRedraw = true;
            }
            break;
    }
}

void navRender() {
    if (!needsRedraw) return;
    needsRedraw = false;

    switch (state) {
        case NAV_DIRECTORY:
            renderDirectory(dirTitles, TEST_PAGE_COUNT, dirIndex);
            break;

        case NAV_PAGE_VIEW:
            renderPage(currentPage);
            break;
    }
}

NavState navGetState() {
    return state;
}
