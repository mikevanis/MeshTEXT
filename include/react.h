#pragma once
#include <stdint.h>

// React types
#define REACT_VISIT  0x00
#define REACT_VOTE_A 0x01
#define REACT_VOTE_B 0x02

// Max unique reactors per page
#define MAX_REACTORS 64

struct ReactTally {
    uint16_t visits;
    uint16_t votes_a;
    uint16_t votes_b;
};

void reactInit();

// Record an incoming react for a local page. Deduplicates by src node.
void reactHandleIncoming(uint8_t page_num, uint32_t src_node, uint8_t react_type);

// Send a react to a remote node
void reactSendVisit(uint32_t dst_node, uint8_t page_num);
void reactSendVote(uint32_t dst_node, uint8_t page_num, uint8_t vote);

// Get tally for a local page
bool reactGetTally(uint8_t page_num, ReactTally& tally);
