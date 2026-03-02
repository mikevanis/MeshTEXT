#include "nav.h"
#include "render.h"
#include "storage.h"
#include <Arduino.h>

static NavState state = NAV_DIRECTORY;
static uint8_t  dirIndex = 0;
static Page     currentPage;
static bool     needsRedraw = true;

static PageListEntry pageList[MAX_PAGES];
static uint8_t       pageCount = 0;
static const char*   dirTitles[MAX_PAGES];

void navRefreshDirectory() {
    pageCount = storageListPages(pageList, MAX_PAGES);
    for (uint8_t i = 0; i < pageCount; i++) {
        dirTitles[i] = pageList[i].title;
    }
    if (dirIndex >= pageCount && pageCount > 0) {
        dirIndex = pageCount - 1;
    }
    needsRedraw = true;
}

void navInit() {
    state = NAV_DIRECTORY;
    dirIndex = 0;
    navRefreshDirectory();
}

void navHandleButton(ButtonEvent evt) {
    if (evt == BTN_NONE) return;

    switch (state) {
        case NAV_DIRECTORY:
            if (pageCount == 0) break;
            if (evt == BTN_SHORT) {
                dirIndex = (dirIndex + 1) % pageCount;
                needsRedraw = true;
            }
            else if (evt == BTN_DOUBLE) {
                if (storageLoadPage(pageList[dirIndex].page_num, currentPage)) {
                    state = NAV_PAGE_VIEW;
                    needsRedraw = true;
                }
            }
            break;

        case NAV_PAGE_VIEW:
            if (evt == BTN_LONG) {
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
            if (pageCount == 0) {
                renderMessage("No pages");
            } else {
                renderDirectory(dirTitles, pageCount, dirIndex);
            }
            break;

        case NAV_PAGE_VIEW:
            renderPage(currentPage);
            break;
    }
}

NavState navGetState() {
    return state;
}
