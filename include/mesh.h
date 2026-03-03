#pragma once
#include <stdint.h>

#define MAX_NEIGHBORS 32

struct Neighbor {
    uint32_t node_id;
    char     name[16];
    uint8_t  category;
    uint8_t  page_count;
    uint8_t  first_page;
    uint8_t  last_page;
    uint32_t last_seen;   // millis()
    float    rssi;
    bool     active;
};

void meshInit();
void meshLoop();

// Neighbor access
uint8_t meshGetNeighbors(Neighbor** list);

// Request a page from a remote node. Returns true if request was sent.
bool meshRequestPage(uint32_t node_id, uint8_t page_num);

// Check if a response is pending/ready
// Returns true if a response arrived, fills page data
struct Page;
bool meshGetResponse(Page& page, bool& timedOut);

// Cache access
bool meshLoadCachedPage(uint32_t node_id, uint8_t page_num, Page& page);
