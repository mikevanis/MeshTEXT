#pragma once
#include <stdint.h>

struct NodeConfig {
    char     name[16];     // user-set node name
    uint8_t  category;     // 0=general, 1=weather, etc.
    uint32_t node_id;      // last 4 bytes of MAC
};

bool configInit();
NodeConfig& configGet();
bool configSave();
uint32_t configGetNodeId();
