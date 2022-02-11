#include <Adafruit_ST7789.h>
#include <stdint.h>

#include "screen.h"
#include "memory.h"

#define ADAFRUIT_2_0
// #define ADAFRUIT_1_13

// Switch these to whatever you need
const uint8_t BACKLIGHT_PIN = 17;
const uint8_t TFT_CS = 8;
const uint8_t TFT_DC = 15;
const uint8_t TFT_RST = 6;

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
boolean backlight_on = false;

void set_backlight(bool turnOn) {
  if (backlight_on != turnOn) {
    digitalWrite(BACKLIGHT_PIN, turnOn ? HIGH : LOW);
    backlight_on = turnOn;
  }
}

// TODO: Create the screen in "init screen"

#define BLANK_CHAR 0x20

void writeCharacter(unsigned char row, unsigned char col, unsigned char val) {
  // The memory has already been updated
  // TODO: This is just to update the visuals
  uint8_t* chr = &fontInfo[val * 8];
  tft.drawBitmap(20 + col * 7, 24 + row * 8, chr, 7, 8, ST77XX_WHITE, ST77XX_BLACK);
}

void init_screen() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  set_backlight(true);
#if defined(ADAFRUIT_1_14)
  tft.init(135, 240);
#elif defined(ADAFRUIT_2_0)
  tft.init(240, 320);
#else
#error Sorry, you need to pick a screen
#endif
  // This is the fastest speed that worked
  // (72mhz also worked, but seemed to be the same speed)
  tft.setSPISpeed(60000000);
  tft.setRotation(1);
}

void clear_screen() {
  init_screen();
  for (unsigned char offs = 0; offs < 8; offs++) {
    memset(&ram[0x400 + offs * 128], BLANK_CHAR, 120); // Draw spaces everywhere
  }
  tft.fillScreen(ST77XX_BLACK);
}

unsigned short row_to_addr(unsigned char row) {
  // bits: 000a bcde =>  0x400 | [00cd eaba b000]
  // Due to range, if !(a && b) is true
  unsigned short ab = (row >> 3) & 3;
  unsigned short cd = (row >> 1) & 3;
  unsigned short e = row & 1;
  return 0x400 | (cd << 8) | (e << 7) | (ab << 5) | (ab << 3);
}

void screenScroll() {
  // move the memory while also updating the screen
  for (unsigned char row = 0; row < 24; row++) {
    memcpy(&ram[row_to_addr(row)], &ram[row_to_addr(row + 1)], 40);
  }
  memset(&ram[row_to_addr(23)], BLANK_CHAR, 40);
  // TODO: Optimize this
  for (unsigned char y = 0; y < 24; y++) {
    for (unsigned char x = 0; x < 40; x++) {
      writeCharacter(y, x, ram[row_to_addr(y) + x]);
    }
  }
}

void screenWrite(unsigned short address, unsigned char value) {
  // Find our row/column
  unsigned char row, col;
  unsigned char block_of_lines = (address >> 7) - 0x08;
  unsigned char block_offset =
    (address & 0x00FF) - ((block_of_lines & 0x01) ? 0x80 : 0x00);
  if (block_offset < 0x28) {
    row = block_of_lines;
    col = block_offset;
  } else if (block_offset < 0x50) {
    row = block_of_lines + 8;
    col = block_offset - 0x28;
  } else {
    row = block_of_lines + 16;
    col = block_offset - 0x50;
  }
  // If not bell character
  if (value != 0x87)
    writeCharacter(row, col, value);
}
