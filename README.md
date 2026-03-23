```
┌────────────────────────────────────────┐
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│░░ ███╗░░░███╗███████╗░██████╗██╗░░██╗░░│
│░░ ████╗░████║██╔════╝██╔════╝██║░░██║░░│
│░░ ██╔████╔██║█████╗░░╚█████╗░███████║░░│
│░░ ██║╚██╔╝██║██╔══╝░░░╚═══██╗██╔══██║░░│
│░░ ██║░╚═╝░██║███████╗██████╔╝██║░░██║░░│
│░░ ╚═╝░░░░░╚═╝╚══════╝╚═════╝░╚═╝░░╚═╝░░│
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
│░░ ████████╗███████╗██╗░░██╗████████╗░░░│
│░░ ╚══██╔══╝██╔════╝╚██╗██╔╝╚══██╔══╝░░░│
│░░░░░░██║░░░█████╗░░░╚███╔╝░░░░██║░░░░░░│
│░░░░░░██║░░░██╔══╝░░░██╔██╗░░░░██║░░░░░░│
│░░░░░░██║░░░███████╗██╔╝╚██╗░░░██║░░░░░░│
│░░░░░░╚═╝░░░╚══════╝╚═╝░░╚═╝░░░╚═╝░░░░░░│
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
├────────────────────────────────────────┤
│  Peer-to-peer teletext mesh network    │
│  No internet, no servers, no accounts. │
└────────────────────────────────────────┘
```

## What is this?

MeshText is a peer-to-peer teletext system built on **ESP32 + LoRa**. Every node is simultaneously a website, browser, editor, and relay. Nodes discover each other over radio, browse each other's pages, and relay traffic for nodes out of direct range.

Pages are 20×8 grids of characters and chunky block graphics — just like 1970s teletext.

```
┌─────────────────────────────────┐
│  ▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄   │
│  █ WELCOME TO MESHTEXT v0.1 █   │
│  ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀   │
│                                 │
│  Peer-to-peer teletext mesh     │
│  on ESP32 + LoRa                │
│                                 │
│  No internet. No servers.       │
└─────────────────────────────────┘
        128x64 OLED output
```

## Hardware

**Heltec LoRa 32 V3** (or V3.1) — Currently only supporting this board (contributions for other boards welcome!)

- ESP32-S3 (WiFi + Bluetooth)
- SX1262 LoRa radio (868 MHz EU only currently)
- 0.96" SSD1306 OLED (128×64)
- USB-C for power and programming
- One user button

## How it works

```
 ┌─────────┐          ┌─────────┐          ┌─────────┐
 │ Node A  │~~LoRa~~~>│ Node B  │~~LoRa~~~>│ Node C  │
 │ 3 pages │<~~relay~~│ 1 page  │<~~relay~~│ 5 pages │
 └─────────┘          └─────────┘          └─────────┘
      │                    │                    │
   ANNOUNCE             ANNOUNCE             ANNOUNCE
   every 60s            every 60s            every 60s
```

1. **Announce** — Each node broadcasts its name and page count every minute
2. **Discover** — Nodes build a neighbor table from announcements
3. **Browse** — Select a neighbor to request their pages over LoRa
4. **Relay** — Packets with TTL > 0 are forwarded, extending range
5. **Cache** — Remote pages are cached locally for offline viewing (TBD whether this stays or not)

## Controls

| Action | Function |
|---|---|
| **Click** | Next item / scroll |
| **Double-click** | Select / enter |
| **Long press** (1s) | Back |
| **Very long press** (3s) | Toggle WiFi edit mode |

## Editing pages

Hold the button for 3 seconds to start WiFi edit mode. The OLED shows the AP name. Connect your phone to the WiFi network and the editor opens automatically.

```
┌──────────────────────────────────┐
│ MeshText Editor                  │
│ ┌──────────────────────────────┐ │
│ │ H e l l o _ w o r l d _ _ _  │ │
│ │ _ _ _ _ _ _ _ _ _ _ _ _ _ _  │ │
│ │ _ _ _ ▄▀▄ _ _ _ _ _ _ _ _ _  │ │
│ │ _ _ _ █▄█ _ _ _ _ _ _ _ _ _  │ │
│ │ _ _ _ _ _ _ _ _ _ _ _ _ _ _  │ │
│ └──────────────────────────────┘ │
│ [Text mode]  [Block mode]  [Save]│
└──────────────────────────────────┘
   
```

The editor is a single HTML page served from the device.

- **Text mode** — tap a cell, type a character
- **Block mode** — tap a cell, toggle 2×3 block graphics
- Manage multiple pages, set titles, configure your node name

## The page format

Each cell is one byte:

```
Bit 7 clear (0x00-0x7F) → ASCII character
Bit 7 set   (0x80-0xBF) → 2×3 block graphic

Block graphic bit layout:
┌───┬───┐
│ 0 │ 1 │    Each bit toggles one
├───┼───┤    rectangle on or off.
│ 2 │ 3 │    6 bits = 64 possible
├───┼───┤    block patterns.
│ 4 │ 5 │
└───┴───┘
```

20 columns × 8 rows = 160 cells = 160 bytes per page. Same technique as BBC Ceefax and ITV Oracle.

## Installing firmware

If you just want to install the firmware on a device without building on PlatformIO, open the flashing tool on Chrome, Opera, or Edge.

https://mikevanis.github.io/MeshTEXT/

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
# Build
pio run

# Upload
pio run -t upload

# Serial monitor
pio device monitor
```

### Serial commands

```
> list                    List all stored pages
> create 101 "News"       Create a new page
> delete 101              Delete a page
> neighbors               Show discovered nodes with RSSI and age
> stress                  Stress test — pick a neighbor, request a page 10 times
> stress 101 20           Stress test page 101, 20 requests
> radiotest               Toggle radio test mode
```

The `stress` command shows a numbered list of discovered neighbors to choose from, then fires repeated page requests and reports success rate, retries, and timing. Use this to test mesh reliability.

## Protocol

Four packet types, flood-routed with deduplication

```
┌─────────────────────────────────────────┐
│ Header (12 bytes)                       │
│ ┌──────┬──────┬──────┬────────┬───────┐ │
│ │ type │ src  │ dst  │ pkt_id │  ttl  │ │
│ │  u8  │ u32  │ u32  │  u16   │  u8   │ │
│ └──────┴──────┴──────┴────────┴───────┘ │
├─────────────────────────────────────────┤
│ ANNOUNCE  "I exist, here's my info"     │
│ REQUEST   "Send me page N"              │
│ RESPONSE  "Here's the page data"        │
│ REACT     "I voted / visited"           │
└─────────────────────────────────────────┘
```

Simple routing - flood with TTL, deduplicate by (src, pkt_id) ring buffer.

## Project structure

```
meshtext/
├── platformio.ini
├── include/
│   ├── pins.h          Pin definitions
│   ├── page.h          Page struct (160 cells + metadata)
│   ├── render.h        OLED renderer
│   ├── buttons.h       Single-button driver
│   ├── nav.h           Navigation state machine
│   ├── storage.h       LittleFS page I/O
│   ├── config.h        Node config
│   ├── webportal.h     WiFi AP + captive portal
│   ├── radio.h         LoRa send/receive
│   ├── protocol.h      Packet structs
│   └── mesh.h          Mesh networking
├── src/
│   ├── main.cpp        Entry point
│   ├── render.cpp      OLED grid renderer
│   ├── buttons.cpp     Button event handler
│   ├── nav.cpp         UI navigation
│   ├── storage.cpp     Flash storage
│   ├── config.cpp      Node settings
│   ├── webportal.cpp   API + editor server
│   ├── radio.cpp       SX1262 driver
│   └── mesh.cpp        Announce, relay, neighbor table
└── data/
    └── editor.html     Browser-based page editor
```

