#include "storage.h"
#include <LittleFS.h>
#include <Arduino.h>

bool storageInit() {
    if (!LittleFS.begin(true)) {  // true = format on first boot
        Serial.println("LittleFS mount failed");
        return false;
    }
    // Ensure /pages directory exists
    if (!LittleFS.exists("/pages")) {
        LittleFS.mkdir("/pages");
    }
    Serial.println("LittleFS mounted");
    return true;
}

static void pagePath(uint8_t num, char* buf) {
    snprintf(buf, 24, "/pages/%03d.bin", num);
}

bool storageSavePage(const Page& page) {
    char path[24];
    pagePath(page.page_num, path);
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    f.write((const uint8_t*)&page, sizeof(Page));
    f.close();
    return true;
}

bool storageLoadPage(uint8_t num, Page& page) {
    char path[24];
    pagePath(num, path);
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    if (f.size() != sizeof(Page)) {
        f.close();
        return false;
    }
    f.read((uint8_t*)&page, sizeof(Page));
    f.close();
    return true;
}

bool storageDeletePage(uint8_t num) {
    char path[24];
    pagePath(num, path);
    return LittleFS.remove(path);
}

uint8_t storageListPages(PageListEntry* list, uint8_t maxEntries) {
    File dir = LittleFS.open("/pages");
    if (!dir || !dir.isDirectory()) return 0;

    uint8_t count = 0;
    File f = dir.openNextFile();
    while (f && count < maxEntries) {
        // Read just enough to get page_num and title
        Page p;
        if (f.size() == sizeof(Page)) {
            f.read((uint8_t*)&p, sizeof(Page));
            list[count].page_num = p.page_num;
            memcpy(list[count].title, p.title, 16);
            count++;
        }
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    // Sort by page_num (simple insertion sort)
    for (uint8_t i = 1; i < count; i++) {
        PageListEntry tmp = list[i];
        int8_t j = i - 1;
        while (j >= 0 && list[j].page_num > tmp.page_num) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = tmp;
    }

    return count;
}
