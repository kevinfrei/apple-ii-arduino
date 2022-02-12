#include <Adafruit_ST7789.h>
#include <stdint.h>

#include "Fonts/FreeSans12pt7b.h"
#include "cpu.h"
#include "memory.h"
#include "screen.h"

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
int charCount = 0;
uint8_t buf[8];
void writeCharacter(unsigned char row, unsigned char col, unsigned char val) {
  charCount++;
  if (row > 23 || col > 39)
    return;
  // The memory has already been updated
  // TODO: This is just to update the visuals
  uint8_t* chr = &fontInfo[val * 8];
  uint8_t buf[8];
  for (int i = 0; i < 8; i++) {
    uint8_t b = chr[i];
    uint8_t d = ((b & 1) << 7) | ((b & 2) << 5) | ((b & 4) << 3) |
                ((b & 8) << 1) | ((b & 16) >> 1) | ((b & 32) >> 3) |
                ((b & 64) >> 5) | ((b & 127) >> 7);
    buf[i] = d;
  }
  tft.drawBitmap(
    20 + col * 7, 48 + row * 8, buf, 7, 8, ST77XX_BLACK, ST77XX_WHITE);
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
  tft.setFont(&FreeSans12pt7b);
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

char buffer[40];
struct bufRect {
  short x1, y1;
  unsigned short w, h;
};

void drawHex(const char* fmt,
             unsigned short x,
             unsigned short y,
             unsigned short val,
             bufRect* oldBuf) {
  sprintf(buffer, fmt, val);
  short x1, y1;
  unsigned short w, h;
  tft.getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
  tft.fillRect(oldBuf->x1, oldBuf->y1, oldBuf->w, oldBuf->h, ST77XX_BLACK);
  *oldBuf = {x1, y1, w, h};
  tft.setCursor(x, y);
  tft.print(buffer);
}

bufRect oA = {0}, oX = {0}, oY = {0}, oPC = {0}, oSR = {0}, oldIPS = {0};
unsigned int total = 0;
unsigned int lastMs = 0;
unsigned char oldA = 0, oldSTP = 0, oldX = 0, oldY = 0, oldSR = 0;
unsigned char oldPC = 0;
void debug_info(unsigned int ms) {
  if (false && (oldA != A || oldX != X || oldY != Y)) {
    if (PC != oldPC) {
      oldPC = PC;
      drawHex("pc.%04X", 2, 26, PC, &oPC);
    }
    if (A != oldA) {
      oldA = A;
      drawHex("a.%02X", 98, 26, A, &oA);
    }
    if (X != oldX) {
      oldX = X;
      drawHex("x.%02x", 150, 26, X, &oX);
    }
    if (Y != oldY) {
      oldY = Y;
      drawHex("y.%02x", 197, 26, Y, &oY);
    }
    if (SR != oldSR) {
      oldSR = SR;
      drawHex("sr.%02x", 244, 26, SR, &oSR);
    }
  }
  if (ms - lastMs >= 1000) {
    lastMs = ms;
    drawHex("CPS: %d", 2, 26, charCount, &oldIPS);
    charCount = 0;
    total = 0;
  }
  total++;
}