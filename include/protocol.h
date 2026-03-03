#pragma once
#include <stdint.h>
#include <string.h>
#include <esp_random.h>

// Packet types
#define PKT_ANNOUNCE  0x01
#define PKT_REQUEST   0x02
#define PKT_RESPONSE  0x03
#define PKT_REACT     0x04

#define BROADCAST_ADDR 0xFFFFFFFF
#define DEFAULT_TTL    3

// 12-byte packet header (little-endian)
struct __attribute__((packed)) PacketHeader {
    uint8_t  type;
    uint32_t src;
    uint32_t dst;
    uint16_t pkt_id;
    uint8_t  ttl;
};

// ANNOUNCE: 32 bytes total (header + 20 bytes payload)
struct __attribute__((packed)) AnnouncePacket {
    PacketHeader hdr;     // 12 bytes
    char     name[16];    // node name
    uint8_t  category;
    uint8_t  page_count;
    uint8_t  first_page;
    uint8_t  last_page;
};

// REQUEST: 13 bytes total
struct __attribute__((packed)) RequestPacket {
    PacketHeader hdr;     // 12 bytes
    uint8_t  page_num;
};

// RESPONSE: 190 bytes total
struct __attribute__((packed)) ResponsePacket {
    PacketHeader hdr;     // 12 bytes
    uint8_t  cells[160];
    uint8_t  page_num;
    uint8_t  flags;
    char     title[16];
};

// REACT: 14 bytes total
struct __attribute__((packed)) ReactPacket {
    PacketHeader hdr;     // 12 bytes
    uint8_t  page_num;
    uint8_t  react_type;  // 0x00=visit, 0x01=vote A, 0x02=vote B
};

// Max packet size (RESPONSE is the largest)
#define MAX_PACKET_SIZE sizeof(ResponsePacket)

// Generate a random packet ID
static inline uint16_t randomPktId() {
    return (uint16_t)(esp_random() & 0xFFFF);
}

// Init a header
static inline void initHeader(PacketHeader& hdr, uint8_t type, uint32_t src, uint32_t dst) {
    hdr.type = type;
    hdr.src = src;
    hdr.dst = dst;
    hdr.pkt_id = randomPktId();
    hdr.ttl = DEFAULT_TTL;
}
