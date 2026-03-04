#include "nav.h"
#include "render.h"
#include "storage.h"
#include "mesh.h"
#include "react.h"
#include "config.h"
#include "page.h"
#include <Arduino.h>

static NavState state = NAV_DIRECTORY;
static uint8_t  dirIndex = 0;
static Page     currentPage;
static bool     needsRedraw = true;

// Track which node owns the current page (0 = local)
static uint32_t currentPageOwner = 0;

// --- Top-level directory: "My Pages" + remote nodes ---
#define MAX_DIR_ENTRIES (1 + MAX_NEIGHBORS)

struct DirEntry {
    const char* title;
    bool isRemote;
    uint32_t node_id;
};

static DirEntry dirEntries[MAX_DIR_ENTRIES];
static uint8_t  dirCount = 0;

// --- Local pages sub-directory ---
static PageListEntry localPageList[MAX_PAGES];
static uint8_t localPageCount = 0;
static uint8_t localPageIndex = 0;
static char localPageTitleBufs[MAX_PAGES][20];

// For remote node browsing
static uint32_t browsingNodeId = 0;
static uint32_t loadingStart = 0;

// Buffers for title strings
static char remoteTitleBufs[MAX_NEIGHBORS][20];

// Visit tracking
static bool visitSent = false;

// --- General selectable element system ---
#define SEL_VOTE_A  0
#define SEL_VOTE_B  1
#define SEL_LINK    2

#define MAX_SELECTABLES 8

struct Selectable {
    uint8_t cellIdx;
    uint8_t type;        // SEL_VOTE_A, SEL_VOTE_B, SEL_LINK
    uint8_t linkTarget;  // target page number (for links only)
};

static Selectable selectables[MAX_SELECTABLES];
static uint8_t selectCount = 0;
static int8_t  selectedIdx = -1;  // -1 = none

static ReactTally currentTally;

// Track what we came from when viewing a page
static bool viewingLocalPage = false;

// Scan page for interactive elements and populate selectables[]
static void initInteractive() {
    selectCount = 0;
    selectedIdx = -1;
    memset(&currentTally, 0, sizeof(currentTally));

    for (int i = 0; i < PAGE_CELLS && selectCount < MAX_SELECTABLES; i++) {
        uint8_t c = currentPage.cells[i];
        if (c == CELL_VOTE_A_BTN) {
            selectables[selectCount++] = { (uint8_t)i, SEL_VOTE_A, 0 };
        } else if (c == CELL_VOTE_B_BTN) {
            selectables[selectCount++] = { (uint8_t)i, SEL_VOTE_B, 0 };
        } else if (c == CELL_LINK && i + 1 < PAGE_CELLS) {
            selectables[selectCount++] = { (uint8_t)i, SEL_LINK, currentPage.cells[i + 1] };
        }
    }

    if (selectCount > 0) selectedIdx = 0;

    // Load tally for local pages
    if (currentPageOwner == 0) {
        reactGetTally(currentPage.page_num, currentTally);
    }
}

// Get the cell index of the currently selected element, or -1
static int16_t getSelectedCellIdx() {
    if (selectedIdx < 0 || selectedIdx >= (int8_t)selectCount) return -1;
    return selectables[selectedIdx].cellIdx;
}

void navRefreshDirectory() {
    dirCount = 0;

    // First entry: "My Pages"
    dirEntries[dirCount].title = "My Pages";
    dirEntries[dirCount].isRemote = false;
    dirEntries[dirCount].node_id = 0;
    dirCount++;

    // Remote nodes from neighbor table
    Neighbor* neighbors;
    uint8_t nCount = meshGetNeighbors(&neighbors);
    uint8_t remoteIdx = 0;
    for (uint8_t i = 0; i < MAX_NEIGHBORS && remoteIdx < nCount; i++) {
        if (!neighbors[i].active) continue;
        uint32_t age = millis() - neighbors[i].last_seen;
        if (age > 1800000) continue;

        snprintf(remoteTitleBufs[remoteIdx], sizeof(remoteTitleBufs[0]),
            "[%s] %.0fdB", neighbors[i].name, neighbors[i].rssi);
        dirEntries[dirCount].title = remoteTitleBufs[remoteIdx];
        dirEntries[dirCount].isRemote = true;
        dirEntries[dirCount].node_id = neighbors[i].node_id;
        dirCount++;
        remoteIdx++;
        if (dirCount >= MAX_DIR_ENTRIES) break;
    }

    if (dirIndex >= dirCount && dirCount > 0) {
        dirIndex = dirCount - 1;
    }
    needsRedraw = true;
}

static void refreshLocalPages() {
    localPageCount = storageListPages(localPageList, MAX_PAGES);
    for (uint8_t i = 0; i < localPageCount; i++) {
        snprintf(localPageTitleBufs[i], sizeof(localPageTitleBufs[0]),
            "%s (%d)", localPageList[i].title, localPageList[i].page_num);
    }
    if (localPageIndex >= localPageCount && localPageCount > 0) {
        localPageIndex = localPageCount - 1;
    }
}

void navInit() {
    state = NAV_DIRECTORY;
    dirIndex = 0;
    navRefreshDirectory();
}

static void enterPageView(const Page& page, uint32_t owner) {
    currentPage = page;
    currentPageOwner = owner;
    state = NAV_PAGE_VIEW;
    visitSent = false;
    viewingLocalPage = (owner == 0);
    needsRedraw = true;

    initInteractive();

    // Auto-send visit react for visitor-tracked remote pages
    if (owner != 0 && (page.flags & PAGE_FLAG_VISITOR)) {
        reactSendVisit(owner, page.page_num);
        visitSent = true;
    }
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
                    // "My Pages" entry
                    refreshLocalPages();
                    localPageIndex = 0;
                    state = NAV_LOCAL_PAGES;
                    needsRedraw = true;
                } else {
                    browsingNodeId = e.node_id;
                    meshRequestPage(e.node_id, 100);
                    state = NAV_LOADING;
                    loadingStart = millis();
                    needsRedraw = true;
                }
            }
            break;

        case NAV_LOCAL_PAGES:
            if (evt == BTN_LONG) {
                state = NAV_DIRECTORY;
                navRefreshDirectory();
            }
            else if (evt == BTN_SHORT) {
                if (localPageCount > 0) {
                    localPageIndex = (localPageIndex + 1) % localPageCount;
                    needsRedraw = true;
                }
            }
            else if (evt == BTN_DOUBLE) {
                if (localPageCount > 0) {
                    if (storageLoadPage(localPageList[localPageIndex].page_num, currentPage)) {
                        enterPageView(currentPage, 0);
                    }
                }
            }
            break;

        case NAV_PAGE_VIEW:
            if (evt == BTN_LONG) {
                if (viewingLocalPage) {
                    refreshLocalPages();
                    state = NAV_LOCAL_PAGES;
                } else {
                    state = NAV_DIRECTORY;
                    navRefreshDirectory();
                }
                needsRedraw = true;
            }
            else if (evt == BTN_SHORT && selectCount > 0) {
                selectedIdx = (selectedIdx + 1) % selectCount;
                needsRedraw = true;
            }
            else if (evt == BTN_DOUBLE && selectCount > 0 && selectedIdx >= 0) {
                Selectable& sel = selectables[selectedIdx];
                if (sel.type == SEL_VOTE_A || sel.type == SEL_VOTE_B) {
                    // Vote logic
                    uint32_t target = (currentPageOwner != 0) ? currentPageOwner : 0;
                    uint8_t vote = (sel.type == SEL_VOTE_A) ? REACT_VOTE_A : REACT_VOTE_B;

                    if (target != 0) {
                        reactSendVote(target, currentPage.page_num, vote);
                    } else {
                        NodeConfig& cfg = configGet();
                        reactHandleIncoming(currentPage.page_num, cfg.node_id, vote);
                        reactGetTally(currentPage.page_num, currentTally);
                    }

                    const char* label = (sel.type == SEL_VOTE_A) ? "Voted A!" : "Voted B!";
                    renderMessage(label);
                    delay(1000);
                    needsRedraw = true;
                }
                else if (sel.type == SEL_LINK) {
                    // Follow hyperlink
                    uint8_t target = sel.linkTarget;
                    if (currentPageOwner == 0) {
                        // Local page link — load from storage
                        Page linkPage;
                        if (storageLoadPage(target, linkPage)) {
                            enterPageView(linkPage, 0);
                        } else {
                            renderMessage("Page not found");
                            delay(1000);
                            needsRedraw = true;
                        }
                    } else {
                        // Remote page link — request over LoRa from same node
                        browsingNodeId = currentPageOwner;
                        meshRequestPage(currentPageOwner, target);
                        state = NAV_LOADING;
                        loadingStart = millis();
                        needsRedraw = true;
                    }
                }
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
        ReactTally remoteTally;
        if (meshGetResponse(currentPage, timedOut, &remoteTally)) {
            enterPageView(currentPage, browsingNodeId);
            currentTally = remoteTally;
            needsRedraw = true;
        } else if (timedOut) {
            renderMessage("Not responding");
            delay(1500);
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

    // Refresh tally for local interactive pages periodically
    if (state == NAV_PAGE_VIEW && currentPageOwner == 0 && selectCount > 0) {
        static uint32_t lastTallyRefresh = 0;
        if (millis() - lastTallyRefresh > 5000) {
            ReactTally newTally;
            if (reactGetTally(currentPage.page_num, newTally)) {
                if (memcmp(&newTally, &currentTally, sizeof(ReactTally)) != 0) {
                    currentTally = newTally;
                    needsRedraw = true;
                }
            }
            lastTallyRefresh = millis();
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

        case NAV_LOCAL_PAGES: {
            if (localPageCount == 0) {
                renderMessage("No local pages");
            } else {
                const char* titles[MAX_PAGES];
                for (uint8_t i = 0; i < localPageCount; i++) {
                    titles[i] = localPageTitleBufs[i];
                }
                renderDirectory(titles, localPageCount, localPageIndex);
            }
            break;
        }

        case NAV_PAGE_VIEW:
            if (selectCount > 0 || (currentPage.flags & (PAGE_FLAG_POLL | PAGE_FLAG_VISITOR))) {
                renderPage(currentPage, &currentTally, getSelectedCellIdx());
            } else {
                renderPage(currentPage);
            }
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
