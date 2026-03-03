#pragma once
#include <stdint.h>

bool radioInit();
bool radioSend(const uint8_t* data, uint8_t len);
int  radioReceive(uint8_t* buf, uint8_t maxLen, float* rssi);
bool isDuplicate(uint32_t src, uint16_t pkt_id);
