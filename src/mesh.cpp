#include "mesh.h"
#include "nav.h"
#include "react.h"
#include "radio.h"
#include "protocol.h"
#include "config.h"
#include "storage.h"
#include "page.h"
#include <Arduino.h>

// Timing
#define ANNOUNCE_INTERVAL  60000   // 1 minute
#define ANNOUNCE_BOOT_DELAY 5000   // 5s after boot
#define STALE_TIMEOUT      1800000 // 30 minutes
#define EVICT_TIMEOUT      7200000 // 2 hours
#define RESPONSE_TIMEOUT   10000   // 10s
#define RELAY_JITTER_MIN   100
#define RELAY_JITTER_MAX   500

// Neighbor table
static Neighbor neighbors[MAX_NEIGHBORS];
static uint8_t neighborCount = 0;

// Announce timing
static uint32_t lastAnnounce = 0;
static bool bootAnnounceSent = false;
static uint32_t bootAnnounceTime = 0;  // randomized boot delay

// Pending request
static bool requestPending = false;
static uint32_t requestTime = 0;
static Page responsePage;
static ReactTally responseTally;
static bool responseReady = false;
static bool responseTimedOut = false;

// Relay queue (simple: one pending relay at a time)
static uint8_t relayBuf[MAX_PACKET_SIZE];
static uint8_t relayLen = 0;
static uint32_t relayTime = 0;

// --- Neighbor table ---

static Neighbor* findNeighbor(uint32_t node_id) {
    for (uint8_t i = 0; i < neighborCount; i++) {
        if (neighbors[i].node_id == node_id && neighbors[i].active) {
            return &neighbors[i];
        }
    }
    return nullptr;
}

static Neighbor* addOrUpdateNeighbor(uint32_t node_id) {
    // Update existing
    Neighbor* n = findNeighbor(node_id);
    if (n) return n;

    // Find empty slot
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (!neighbors[i].active) {
            neighbors[i].active = true;
            neighbors[i].node_id = node_id;
            if (i >= neighborCount) neighborCount = i + 1;
            return &neighbors[i];
        }
    }

    // Evict oldest
    uint32_t oldest = UINT32_MAX;
    uint8_t oldestIdx = 0;
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].last_seen < oldest) {
            oldest = neighbors[i].last_seen;
            oldestIdx = i;
        }
    }
    neighbors[oldestIdx].active = true;
    neighbors[oldestIdx].node_id = node_id;
    return &neighbors[oldestIdx];
}

static void evictStaleNeighbors() {
    uint32_t now = millis();
    for (uint8_t i = 0; i < neighborCount; i++) {
        if (!neighbors[i].active) continue;
        if ((now - neighbors[i].last_seen) > EVICT_TIMEOUT) {
            neighbors[i].active = false;
        }
    }
}



// --- Send ---

static void sendAnnounce() {
    NodeConfig& cfg = configGet();

    PageListEntry list[MAX_PAGES];
    uint8_t count = storageListPages(list, MAX_PAGES);

    AnnouncePacket pkt;
    initHeader(pkt.hdr, PKT_ANNOUNCE, cfg.node_id, BROADCAST_ADDR);
    strlcpy(pkt.name, cfg.name, sizeof(pkt.name));
    pkt.category = cfg.category;
    pkt.page_count = count;
    pkt.first_page = count > 0 ? list[0].page_num : 0;
    pkt.last_page = count > 0 ? list[count - 1].page_num : 0;

    radioSend((uint8_t*)&pkt, sizeof(pkt));
    lastAnnounce = millis();
    Serial.printf("TX ANNOUNCE: %s (%d pages)\n", cfg.name, count);
}

// --- Receive handlers ---

static void handleAnnounce(const AnnouncePacket& ann, float rssi) {
    Neighbor* n = addOrUpdateNeighbor(ann.hdr.src);
    strlcpy(n->name, ann.name, sizeof(n->name));
    n->category = ann.category;
    n->page_count = ann.page_count;
    n->first_page = ann.first_page;
    n->last_page = ann.last_page;
    n->last_seen = millis();
    n->rssi = rssi;

    Serial.printf("Neighbor: %s (%08X) %d pages rssi=%.0f\n",
        n->name, n->node_id, n->page_count, rssi);

    // Refresh directory so new/updated neighbors appear on OLED
    if (navGetState() == NAV_DIRECTORY) {
        navRefreshDirectory();
    }
}

static void handleRequest(const RequestPacket& req) {
    NodeConfig& cfg = configGet();
    if (req.hdr.dst != cfg.node_id) return;

    Page p;
    if (!storageLoadPage(req.page_num, p)) {
        Serial.printf("REQ for page %d - not found\n", req.page_num);
        return;
    }

    ResponsePacket resp;
    memset(&resp, 0, sizeof(resp));
    initHeader(resp.hdr, PKT_RESPONSE, cfg.node_id, req.hdr.src);
    memcpy(resp.cells, p.cells, PAGE_CELLS);
    resp.page_num = p.page_num;
    resp.flags = p.flags;
    memcpy(resp.title, p.title, 16);

    // Include tally data if page has interactive elements
    ReactTally tally;
    if (reactGetTally(p.page_num, tally)) {
        resp.visits = tally.visits;
        resp.votes_a = tally.votes_a;
        resp.votes_b = tally.votes_b;
    }

    radioSend((uint8_t*)&resp, sizeof(resp));
    Serial.printf("TX RESPONSE page %d to %08X\n", p.page_num, req.hdr.src);
}

static void handleResponse(const ResponsePacket& resp) {
    NodeConfig& cfg = configGet();
    if (resp.hdr.dst != cfg.node_id) return;

    // If we have a pending request, fulfill it
    if (requestPending) {
        memcpy(responsePage.cells, resp.cells, PAGE_CELLS);
        responsePage.page_num = resp.page_num;
        responsePage.flags = resp.flags;
        memcpy(responsePage.title, resp.title, 16);
        responseTally.visits = resp.visits;
        responseTally.votes_a = resp.votes_a;
        responseTally.votes_b = resp.votes_b;
        responseReady = true;
        requestPending = false;
        Serial.printf("RX RESPONSE page %d from %08X\n", resp.page_num, resp.hdr.src);
    }
}

static void handleReact(const ReactPacket& react) {
    NodeConfig& cfg = configGet();
    if (react.hdr.dst != cfg.node_id) return;

    // Only accept reacts for pages we own
    Page p;
    if (!storageLoadPage(react.page_num, p)) return;

    reactHandleIncoming(react.page_num, react.hdr.src, react.react_type);
}

// --- Relay ---

static void scheduleRelay(const uint8_t* data, uint8_t len, uint8_t newTtl) {
    if (relayLen > 0) return;  // already have one pending
    memcpy(relayBuf, data, len);
    // Update TTL in the copy
    PacketHeader* hdr = (PacketHeader*)relayBuf;
    hdr->ttl = newTtl;
    relayLen = len;
    relayTime = millis() + random(RELAY_JITTER_MIN, RELAY_JITTER_MAX);
}

static void processRelay() {
    if (relayLen == 0) return;
    if (millis() < relayTime) return;

    radioSend(relayBuf, relayLen);
    Serial.printf("RELAY pkt_id=%04X ttl=%d\n",
        ((PacketHeader*)relayBuf)->pkt_id,
        ((PacketHeader*)relayBuf)->ttl);
    relayLen = 0;
}

// --- Main receive ---

static void processPacket(const uint8_t* buf, uint8_t len, float rssi) {
    if (len < sizeof(PacketHeader)) return;
    const PacketHeader* hdr = (const PacketHeader*)buf;
    NodeConfig& cfg = configGet();

    // Don't process our own packets
    if (hdr->src == cfg.node_id) return;

    // Dedup
    if (isDuplicate(hdr->src, hdr->pkt_id)) return;

    // Process if for us or broadcast
    bool forUs = (hdr->dst == cfg.node_id || hdr->dst == BROADCAST_ADDR);

    if (forUs) {
        switch (hdr->type) {
            case PKT_ANNOUNCE:
                if (len >= sizeof(AnnouncePacket))
                    handleAnnounce(*(const AnnouncePacket*)buf, rssi);
                break;
            case PKT_REQUEST:
                if (len >= sizeof(RequestPacket))
                    handleRequest(*(const RequestPacket*)buf);
                break;
            case PKT_RESPONSE:
                if (len >= sizeof(ResponsePacket))
                    handleResponse(*(const ResponsePacket*)buf);
                break;
            case PKT_REACT:
                if (len >= sizeof(ReactPacket))
                    handleReact(*(const ReactPacket*)buf);
                break;
        }
    }

    // Relay if TTL > 0
    if (hdr->ttl > 0) {
        scheduleRelay(buf, len, hdr->ttl - 1);
    }
}

// --- Public API ---

void meshInit() {
    memset(neighbors, 0, sizeof(neighbors));
    neighborCount = 0;
    requestPending = false;
    responseReady = false;
    relayLen = 0;
    bootAnnounceTime = ANNOUNCE_BOOT_DELAY + random(0, 3000);  // 5-8s jitter
}

void meshLoop() {
    uint32_t now = millis();

    // Boot announce
    if (!bootAnnounceSent && now >= bootAnnounceTime) {
        sendAnnounce();
        bootAnnounceSent = true;
    }

    // Periodic announce
    if (bootAnnounceSent && (now - lastAnnounce) >= ANNOUNCE_INTERVAL) {
        sendAnnounce();
    }

    // Receive packets
    uint8_t buf[MAX_PACKET_SIZE];
    float rssi;
    int len = radioReceive(buf, sizeof(buf), &rssi);
    if (len > 0) {
        processPacket(buf, len, rssi);
    }

    // Process pending relay
    processRelay();

    // Evict stale neighbors periodically
    static uint32_t lastEvict = 0;
    if ((now - lastEvict) > 60000) {
        evictStaleNeighbors();
        lastEvict = now;
    }

    // Request timeout
    if (requestPending && (now - requestTime) > RESPONSE_TIMEOUT) {
        requestPending = false;
        responseTimedOut = true;
    }
}

uint8_t meshGetNeighbors(Neighbor** list) {
    *list = neighbors;
    // Count active
    uint8_t count = 0;
    for (uint8_t i = 0; i < neighborCount; i++) {
        if (neighbors[i].active) count++;
    }
    return count;
}

bool meshRequestPage(uint32_t node_id, uint8_t page_num) {
    NodeConfig& cfg = configGet();

    RequestPacket pkt;
    initHeader(pkt.hdr, PKT_REQUEST, cfg.node_id, node_id);
    pkt.page_num = page_num;

    requestPending = true;
    requestTime = millis();
    responseReady = false;
    responseTimedOut = false;

    bool ok = radioSend((uint8_t*)&pkt, sizeof(pkt));
    Serial.printf("TX REQUEST page %d from %08X\n", page_num, node_id);
    return ok;
}

bool meshGetResponse(Page& page, bool& timedOut, ReactTally* tally) {
    if (responseReady) {
        page = responsePage;
        if (tally) *tally = responseTally;
        responseReady = false;
        timedOut = false;
        return true;
    }
    if (responseTimedOut) {
        responseTimedOut = false;
        timedOut = true;
        return false;
    }
    timedOut = false;
    return false;
}
