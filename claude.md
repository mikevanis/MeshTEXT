# MeshText

A peer-to-peer teletext mesh on ESP32+LoRa. Every node is a website, browser, editor, and relay. No internet. No servers. No accounts. You exist on the network by carrying a powered-on device.

## Hardware

The reference board is the **Heltec LoRa 32 V3** (also V3.1, same firmware):
- **MCU**: ESP32-S3FN8 (WiFi & Bluetooth)
- **LoRa**: Semtech SX1262
- **Display**: 0.96" SSD1306 OLED, 128x64, I2C (SDA=GPIO41, SCL=GPIO42)
- **Buttons**: USER (GPIO0) and RST
- **Frequency**: 863-870 MHz (EU) / 902-928 MHz (US)
- **USB**: USB-C (note: use USB-A to USB-C cable for battery charging, no ESD protection on CP2102)
- **Antenna**: U.FL/IPEX connector for LoRa, dedicated 2.4 GHz metal spring antenna for WiFi/BT

Note: the V3 only has one user button (GPIO0) plus reset. We need to treat reset as the second button or wire an external button to a free GPIO.

---

## The Page

A page is a 20x8 monochrome grid. Each of the 160 cells is one byte. There are two types of cell:

**Text cell** (bit 7 clear, 0x00-0x7F): displays a printable ASCII character — a letter, number, or symbol. This is how you write words.

**Block cell** (bit 7 set, 0x80-0xBF): displays a 2x3 grid of chunky rectangles, each toggled on or off. This is how you draw graphics. Bits 0-5 of the byte map to the six rectangles:

```
+---+---+
| 0 | 1 |
+---+---+
| 2 | 3 |
+---+---+
| 4 | 5 |
+---+---+
```

Line up block cells across the screen and you can draw shapes, borders, icons, maps — crude pixel art at 40x24 resolution. This is the same technique original teletext used for all its graphics.

One byte, one cell, self-describing. No separate bitmask needed.

A page on disk:

```
cells[160]    The grid, row-major
page_num      uint8, 1-255 (0 = reserved for directory)
flags         uint8, bit 0 = auto-cycle, bit 1 = poll, bit 2 = visitor tracking
title[16]     Null-terminated, shown in directory listings
              -------
              178 bytes
```

## The Node

Each node has:
- **id**: last 4 bytes of ESP32 MAC. Displayed as hex (e.g. "A3F0B21C").
- **name**: 16 chars, user-set via captive portal.
- **category**: one byte (0=general, 1=weather, 2=community, 3=personal, 4=info, 5=fun, 6=sensor).

Stored as `/config.json` on LittleFS. Pages stored as `/pages/NNN.bin`.

## The Protocol

Three packet types for the core, plus one for interactions.

Every packet has this header (12 bytes):

```
type        uint8     0x01=ANNOUNCE, 0x02=REQUEST, 0x03=RESPONSE, 0x04=REACT
src         uint32    sender node id
dst         uint32    target node id (0xFFFFFFFF = broadcast)
pkt_id      uint16    random, for dedup
ttl         uint8     hops remaining, default 3
```

**ANNOUNCE** (32 bytes total, broadcast every 5 min):
```
header[12]    type=0x01, dst=0xFFFFFFFF
name[16]      node name
category      uint8
page_count    uint8
first_page    uint8
last_page     uint8
```

**REQUEST** (13 bytes total, unicast):
```
header[12]    type=0x02
page_num      uint8
```

**RESPONSE** (190 bytes total, unicast):
```
header[12]    type=0x03
cells[160]    page cell data
page_num      uint8
flags         uint8
title[16]     page title
```

**REACT** (15 bytes total, unicast):
```
header[12]    type=0x04
page_num      uint8
react_type    uint8     0x00=visit ("I was here"), 0x01=vote A, 0x02=vote B
```

### Interactive Pages

A page with the poll flag (bit 1 of flags) is a poll. The OLED shows the question and two options. Press A to vote for the first, B for the second. The node sends a REACT packet and the hosting node tallies it. The page auto-updates with counts.

A page with the visitor flag (bit 2 of flags) tracks visits. When you view it, your node sends a REACT with type=visit. The hosting node records it. The page shows a visitor count or the last few visitor node names.

Hosting nodes keep tallies in a small file alongside the page: `/pages/NNN.react`. They deduplicate by source node_id — one vote or visit per node per page.

### Routing

Flood with dedup. On receive:
1. Check pkt_id against last 64 seen. If duplicate, drop.
2. If it's for us, process it.
3. If TTL > 0, decrement and relay after random 100-500ms jitter.

That's the entire networking stack.

### Neighbor Table

In RAM. Max 32 entries. Updated from ANNOUNCEs. Each entry: node_id, name, category, page range, last_seen millis, rssi. Stale after 30 min. Evicted after 2 hours.

## The Display

128x64 OLED. Each cell is 6x8 pixels. 20 cells x 6px = 120px wide (4px padding each side). 8 cells x 8px = 64px tall (exact fit).

Text cells: render with any 6x8 monospace font (u8g2 has many).
Block cells: each of the six rectangles is 3x2 pixels within the 6x8 cell.

### Navigation (two buttons)

```
Short A     Next item / scroll down
Short B     Select / enter
Long A      Back
Long B      Toggle auto-cycle
Both held   Enter/exit WiFi edit mode
```

Flow: Directory -> select node -> their page 100 (index) -> select page -> view page. Long A goes back one level. Directory is auto-generated from local pages + neighbor table.

## The Editor

Both buttons held starts a WiFi AP: "MeshTxt-XXXX". Phone connects, captive portal opens the editor. The editor is a single HTML file served from flash. No external dependencies, no frameworks. Must work on mobile.

### API

```
GET    /              editor.html (gzipped)
GET    /api/config    {name, category}
PUT    /api/config    update name/category
GET    /api/pages     [{page_num, title, flags}]
GET    /api/page/:n   {page_num, flags, title, cells: [160 ints]}
PUT    /api/page/:n   save page
DELETE /api/page/:n   delete page
```

### Editor UI

- 20x8 grid, tap to select cell
- Text mode: type a character, it's placed, cursor advances
- Block mode: cell shows 2x3 toggle grid for the six rectangles
- Toggle between modes with a button
- Page list: switch pages, add, delete, set number and title
- Page flags: toggles for auto-cycle, poll, and visitor tracking
- Preview: pixel-accurate OLED rendering (white on black)
- Node settings: name, category
- Save: PUT to API, visual confirmation

LoRa mesh keeps running while WiFi is active (separate radios).

---

## Build Plan

Six phases. Each phase ends with a concrete test. Don't move on until it passes.

### Phase 1: Display and Input

Set up PlatformIO project. Initialize OLED. Define the page struct. Write the renderer (iterate 20x8, draw text or block per cell). Create 3-4 hardcoded test pages. Implement two-button driver with debounce and short/long press. Build the navigation state machine: directory listing -> page view -> back.

**Test**: Boot shows directory of hardcoded pages. A scrolls, B selects and shows page content with correct text and block rendering. Long A returns to directory.

### Phase 2: Storage

Initialize LittleFS. Implement save/load/delete/list for page files. On first boot, create a default page 100. Replace hardcoded pages with filesystem reads. Directory auto-generates from stored pages.

**Test**: First boot creates and displays default page. Add a page via serial command. Reboot — both pages persist in directory.

### Phase 3: Captive Portal and Editor

Start WiFi AP on both-buttons-held. Serve captive portal. Build the full editor.html — build and test as a standalone HTML file in a desktop browser first, then embed in firmware. Implement all API endpoints. OLED shows edit mode with SSID.

**Test**: Connect phone to AP, editor loads. Create a page with text and block art. Save. Exit edit mode. Page appears on OLED correctly. Re-enter edit mode, page loads in editor as saved. Test on iOS Safari and Android Chrome.

### Phase 4: LoRa Radio

Initialize LoRa via RadioLib. Implement send/receive with the 12-byte packet header. Implement the 64-entry dedup ring buffer.

**Test**: Two boards. One sends test packets every 5 seconds. Other receives and prints to serial with RSSI. Duplicate pkt_ids are dropped.

### Phase 5: Mesh

Implement ANNOUNCE broadcast (every 5 min, plus 10s after boot). Implement neighbor table. Implement flood relay with jitter. Implement REQUEST/RESPONSE: selecting a remote node in the directory sends REQUEST for their page 100, response is displayed and cached to `/cache/<nodeid>/NNN.bin`. Directory now shows local pages + remote nodes.

**Test (two nodes)**: Both boot, discover each other via announces, appear in each other's directory. Node A browses node B's pages. Power off B — pages still viewable from cache.

**Test (three nodes)**: A and C out of range. B relays. A discovers and fetches pages from C through B.

### Phase 6: Polish and Interactions

Auto-cycle mode (long B toggles, cycles through cached pages). Loading spinner while waiting for RESPONSE. Timeout after 10s with "not responding" message. RSSI indicator next to nodes in directory. Implement REACT packets: polls (A/B vote on poll pages) and visitor pings (auto-sent when viewing a visitor-tracked page). Hosting nodes tally reacts, deduped by source node. Editor gets poll/visitor flag toggles per page.

**Test**: Auto-cycle rotates pages on OLED. Request to offline node times out gracefully. Node A creates a poll page. Node B views it and votes with A button. Node A's tally updates. Node B votes again — deduplicated, count doesn't change.

---

## Project Structure

```
meshtext/
  platformio.ini
  src/
    main.cpp
    pins.h            Pin definitions per board (ifdef)
    page.h            Page struct
    render.cpp        OLED grid renderer
    buttons.cpp       Two-button driver
    nav.cpp           Navigation state machine
    storage.cpp       LittleFS page I/O
    config.cpp        Node config
    wifi.cpp          Captive portal and API
    radio.cpp         LoRa send/receive/dedup
    protocol.h        Packet structs
    mesh.cpp          Announce, relay, neighbor table
    react.cpp         Poll tallies, visitor tracking
  data/
    editor.html
```

## platformio.ini

```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
framework = arduino
lib_deps =
    u8g2
    jgromes/RadioLib
    me-no-dev/ESPAsyncWebServer
    me-no-dev/AsyncTCP
    bblanchon/ArduinoJson
monitor_speed = 115200
board_build.filesystem = littlefs
```

## Pins (Heltec LoRa 32 V3)

```cpp
// OLED (I2C)
#define OLED_SDA  41
#define OLED_SCL  42
#define OLED_RST  21

// LoRa (SPI — SX1262)
#define LORA_CS    8
#define LORA_RST  12
#define LORA_DIO1 14
#define LORA_BUSY 13
#define LORA_SCK   9
#define LORA_MOSI 10
#define LORA_MISO 11

// Buttons
#define BTN_A      0   // USER button (active low, active on Heltec V3)
// BTN_B: TBD — V3 only has one user button. Options:
//   1. Wire external button to a free GPIO (e.g. GPIO33, GPIO34)
//   2. Use long/short/double-tap on single button (degrades UX)
//   3. Use touch pin input on an exposed GPIO
```

## LoRa Settings

SX1262 via RadioLib. 868 MHz (EU) / 915 MHz (US). SF7, 125 kHz BW, CR 4/5. Raw packets, no LoRaWAN. Note: SX1262 uses DIO1 and BUSY pins instead of the DIO0 used by older SX1276 chips.

## Conventions

- Little-endian for multi-byte integers in packets.
- Page 0 is reserved. User pages 1-255.
- Node IDs displayed as 8-char uppercase hex.
- All timing via millis(), no RTC.
- editor.html: zero external deps, vanilla JS, inline CSS, target under 50KB uncompressed.
- One phase = one commit. Tests pass before moving on.
