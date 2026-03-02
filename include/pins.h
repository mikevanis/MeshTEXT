#pragma once

// Heltec LoRa 32 V3 pin definitions

// OLED (I2C, SSD1306 128x64)
#define OLED_SDA  17
#define OLED_SCL  18
#define OLED_RST  21
#define OLED_VEXT 36  // Vext power gate — LOW to enable OLED power

// LoRa (SPI, SX1262)
#define LORA_CS    8
#define LORA_RST  12
#define LORA_DIO1 14
#define LORA_BUSY 13
#define LORA_SCK   9
#define LORA_MOSI 10
#define LORA_MISO 11

// Buttons
#define BTN_A      0  // USER button (active low)
