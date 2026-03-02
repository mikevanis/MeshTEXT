#pragma once
#include "page.h"
#include <stdint.h>

#define MAX_PAGES 32

struct PageListEntry {
    uint8_t page_num;
    char    title[16];
};

bool storageInit();
bool storageSavePage(const Page& page);
bool storageLoadPage(uint8_t num, Page& page);
bool storageDeletePage(uint8_t num);
uint8_t storageListPages(PageListEntry* list, uint8_t maxEntries);
