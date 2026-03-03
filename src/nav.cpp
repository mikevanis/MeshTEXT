#include "nav.h"
#include "render.h"
#include "storage.h"
#include "mesh.h"
#include "page.h"
#include <Arduino.h>

static NavState state = NAV_DIRECTORY;
static uint8_t  dirIndex = 0;
static Page     currentPage;
static bool     needsRedraw = true;

// Directory: local pages + remote nodes
#define MAX_DIR_ENTRIES (MAX_PAGES + MAX_NEIGHBORS)

struct DirEntry {
    const char* title;
    bool isRemote;
    uint32_t node_id;     // for remote nodes
    uint8_t  page_num;    // for local pages
};

static DirEntry dirEntries[MAX_DIR_ENTRIES];
static uint8_t  dirCount = 0;

// For remote node browsing
static uint32_t browsingNodeId = 0;
static uint32_t loadingStart = 0;

// Buffers for title strings
static char localTitleBufs[MAX_PAGES][16];
static char remoteTitleBufs[MAX_NEIGHBORS][20];  // "Name (RSSI)"

void navRefreshDirectory() {
    dirCount = 0;

    // Local pages
    PageListEntry pageList[MAX_PAGES];
    uint8_t pageCount = storageListPages(pageList, MAX_PAGES);
    for (uint8_t i = 0; i < pageCount && dirCount < MAX_DIR_ENTRIES; i++) {
        strlcpy(localTitleBufs[i], pageList[i].title, 16);
        dirEntries[dirCount].title = localTitleBufs[i];
        dirEntries[dirCount].isRemote = false;
        dirEntries[dirCount].page_num = pageList[i].page_num;
        dirCount++;
    }

    // Remote nodes from neighbor table
    Neighbor* neighbors;
    uint8_t nCount = meshGetNeighbors(&neighbors);
    uint8_t remoteIdx = 0;
    for (uint8_t i = 0; i < MAX_NEIGHBORS && remoteIdx < nCount; i++) {
        if (!neighbors[i].active) continue;
        uint32_t age = millis() - neighbors[i].last_seen;
        if (age > 1800000) continue;  // skip stale

        snprintf(remoteTitleBufs[remoteIdx], sizeof(remoteTitleBufs[0]),
            "[%s] %.0fdB", neighbors[i].name, neighbors[i].rssi);
        dirEntries[dirCount].title = remoteTitleBufs[remoteIdx];
        dirEntries[dirCount].isRemote = true;
        dirEntries[dirCount].node_id = neighbors[i].node_id;
        dirCount++;
        remoteIdx++;
    }

    if (dirIndex >= dirCount && dirCount > 0) {
        dirIndex = dirCount - 1;
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
            if (dirCount == 0) break;
            if (evt == BTN_SHORT) {
                dirIndex = (dirIndex + 1) % dirCount;
                needsRedraw = true;
            }
            else if (evt == BTN_DOUBLE) {
                DirEntry& e = dirEntries[dirIndex];
                if (!e.isRemote) {
                    // Local page
                    if (storageLoadPage(e.page_num, currentPage)) {
                        state = NAV_PAGE_VIEW;
                        needsRedraw = true;
                    }
                } else {
                    // Remote node — try cache first, else request
                    browsingNodeId = e.node_id;
                    if (meshLoadCachedPage(e.node_id, 100, currentPage)) {
                        state = NAV_PAGE_VIEW;
                        needsRedraw = true;
                    } else {
                        meshRequestPage(e.node_id, 100);
                        state = NAV_LOADING;
                        loadingStart = millis();
                        needsRedraw = true;
                    }
                }
            }
            break;

        case NAV_PAGE_VIEW:
            if (evt == BTN_LONG) {
                state = NAV_DIRECTORY;
                navRefreshDirectory();
            }
            break;

        case NAV_LOADING:
            if (evt == BTN_LONG) {
                state = NAV_DIRECTORY;
                needsRedraw = true;
            }
            break;

        case NAV_NODE_PAGES:
            break;
    }
}

void navLoop() {
    if (state == NAV_LOADING) {
        bool timedOut;
        if (meshGetResponse(currentPage, timedOut)) {
            state = NAV_PAGE_VIEW;
            needsRedraw = true;
        } else if (timedOut) {
            renderMessage("Not responding", "Long press = back");
            state = NAV_PAGE_VIEW;  // show the message, long press goes back
            // Actually, let's go back to directory
            state = NAV_DIRECTORY;
            needsRedraw = true;
        }

        // Animate loading spinner
        static uint32_t lastSpin = 0;
        static uint8_t spinFrame = 0;
        if (millis() - lastSpin > 200) {
            const char* frames[] = {"|", "/", "-", "\\"};
            char msg[32];
            snprintf(msg, sizeof(msg), "Loading... %s", frames[spinFrame % 4]);
            renderMessage(msg);
            spinFrame++;
            lastSpin = millis();
        }
    }
}

void navRender() {
    if (!needsRedraw) return;
    needsRedraw = false;

    switch (state) {
        case NAV_DIRECTORY: {
            if (dirCount == 0) {
                renderMessage("No pages");
            } else {
                const char* titles[MAX_DIR_ENTRIES];
                for (uint8_t i = 0; i < dirCount; i++) {
                    titles[i] = dirEntries[i].title;
                }
                renderDirectory(titles, dirCount, dirIndex);
            }
            break;
        }

        case NAV_PAGE_VIEW:
            renderPage(currentPage);
            break;

        case NAV_LOADING:
            renderMessage("Loading...");
            break;

        case NAV_NODE_PAGES:
            break;
    }
}

NavState navGetState() {
    return state;
}
