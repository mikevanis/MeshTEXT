#include "react.h"
#include "radio.h"
#include "protocol.h"
#include "config.h"
#include <Arduino.h>
#include <LittleFS.h>

// On-disk format for react file: array of {node_id, react_type}
struct __attribute__((packed)) ReactEntry {
    uint32_t node_id;
    uint8_t  react_type;
};

static void reactPath(uint8_t page_num, char* buf, size_t len) {
    snprintf(buf, len, "/pages/%03d.react", page_num);
}

void reactInit() {
    // Nothing to init — files are per-page on LittleFS
}

void reactHandleIncoming(uint8_t page_num, uint32_t src_node, uint8_t react_type) {
    char path[24];
    reactPath(page_num, path, sizeof(path));

    // Read existing entries to check for duplicates
    ReactEntry entries[MAX_REACTORS];
    uint16_t count = 0;

    File f = LittleFS.open(path, "r");
    if (f) {
        count = f.size() / sizeof(ReactEntry);
        if (count > MAX_REACTORS) count = MAX_REACTORS;
        f.read((uint8_t*)entries, count * sizeof(ReactEntry));
        f.close();
    }

    // Check if this node already reacted with this type
    for (uint16_t i = 0; i < count; i++) {
        if (entries[i].node_id == src_node && entries[i].react_type == react_type) {
            Serial.printf("REACT dedup: node %08X already sent type %d for page %d\n",
                src_node, react_type, page_num);
            return;
        }
    }

    // Append new entry
    if (count >= MAX_REACTORS) {
        Serial.println("REACT file full");
        return;
    }

    f = LittleFS.open(path, "a");
    if (!f) return;
    ReactEntry entry = { src_node, react_type };
    f.write((const uint8_t*)&entry, sizeof(entry));
    f.close();

    Serial.printf("REACT recorded: node %08X type %d for page %d\n",
        src_node, react_type, page_num);
}

void reactSendVisit(uint32_t dst_node, uint8_t page_num) {
    NodeConfig& cfg = configGet();
    ReactPacket pkt;
    initHeader(pkt.hdr, PKT_REACT, cfg.node_id, dst_node);
    pkt.page_num = page_num;
    pkt.react_type = REACT_VISIT;
    radioSend((uint8_t*)&pkt, sizeof(pkt));
    Serial.printf("TX REACT visit page %d to %08X\n", page_num, dst_node);
}

void reactSendVote(uint32_t dst_node, uint8_t page_num, uint8_t vote) {
    NodeConfig& cfg = configGet();
    ReactPacket pkt;
    initHeader(pkt.hdr, PKT_REACT, cfg.node_id, dst_node);
    pkt.page_num = page_num;
    pkt.react_type = vote;
    radioSend((uint8_t*)&pkt, sizeof(pkt));
    Serial.printf("TX REACT vote %d page %d to %08X\n", vote, page_num, dst_node);
}

bool reactGetTally(uint8_t page_num, ReactTally& tally) {
    tally.visits = 0;
    tally.votes_a = 0;
    tally.votes_b = 0;

    char path[24];
    reactPath(page_num, path, sizeof(path));

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    uint16_t count = f.size() / sizeof(ReactEntry);
    ReactEntry entry;
    for (uint16_t i = 0; i < count; i++) {
        if (f.read((uint8_t*)&entry, sizeof(entry)) != sizeof(entry)) break;
        switch (entry.react_type) {
            case REACT_VISIT:  tally.visits++;  break;
            case REACT_VOTE_A: tally.votes_a++; break;
            case REACT_VOTE_B: tally.votes_b++; break;
        }
    }
    f.close();
    return true;
}
