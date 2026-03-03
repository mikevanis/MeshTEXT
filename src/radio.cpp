#include "radio.h"
#include "pins.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

// SX1262 on Heltec V3: custom SPI pins
static SPIClass radioSPI(HSPI);
static SX1262 radio = new Module(LORA_CS, LORA_DIO1, LORA_RST, LORA_BUSY, radioSPI);

// Flag set by DIO1 interrupt when a packet arrives
static volatile bool rxFlag = false;

static void IRAM_ATTR onDio1() {
    rxFlag = true;
}

// Dedup ring buffer
#define DEDUP_SIZE 64
struct DedupEntry {
    uint32_t src;
    uint16_t pkt_id;
};
static DedupEntry dedupBuf[DEDUP_SIZE];
static uint8_t dedupIdx = 0;

bool radioInit() {
    radioSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

    Serial.print("Radio init... ");
    int state = radio.begin(
        868.0,    // frequency MHz
        125.0,    // bandwidth kHz
        7,        // spreading factor
        5,        // coding rate (4/5)
        0x12,     // sync word
        10,       // output power dBm
        8,        // preamble length
        1.8,      // TCXO voltage (Heltec V3 uses 1.8V TCXO)
        false     // use LDO regulator
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("FAILED (%d)\n", state);
        return false;
    }

    // Heltec V3: DIO2 controls the RF switch
    radio.setDio2AsRfSwitch(true);

    // Set DIO1 interrupt for RX done
    radio.setDio1Action(onDio1);

    // Start listening
    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("RX start failed (%d)\n", state);
        return false;
    }

    // Clear dedup buffer
    memset(dedupBuf, 0, sizeof(dedupBuf));

    Serial.println("OK (868 MHz, SF7, 125kHz)");
    return true;
}

bool radioSend(const uint8_t* data, uint8_t len) {
    int state = radio.transmit(data, len);

    // Clear any flag set by TX done, go back to receive
    rxFlag = false;
    radio.startReceive();

    return (state == RADIOLIB_ERR_NONE);
}

int radioReceive(uint8_t* buf, uint8_t maxLen, float* rssi) {
    // Only process if DIO1 interrupt fired (new packet available)
    if (!rxFlag) return 0;
    rxFlag = false;

    int len = radio.getPacketLength();
    if (len == 0) {
        radio.startReceive();
        return 0;
    }

    int readLen = len > maxLen ? maxLen : len;
    int state = radio.readData(buf, readLen);

    if (state != RADIOLIB_ERR_NONE) {
        radio.startReceive();
        return 0;
    }

    if (rssi) {
        *rssi = radio.getRSSI();
    }

    // Restart receive for next packet
    radio.startReceive();

    return readLen;
}

bool isDuplicate(uint32_t src, uint16_t pkt_id) {
    // Check against existing entries
    for (uint8_t i = 0; i < DEDUP_SIZE; i++) {
        if (dedupBuf[i].src == src && dedupBuf[i].pkt_id == pkt_id) {
            return true;
        }
    }

    // Not found — add to ring buffer
    dedupBuf[dedupIdx].src = src;
    dedupBuf[dedupIdx].pkt_id = pkt_id;
    dedupIdx = (dedupIdx + 1) % DEDUP_SIZE;

    return false;
}
