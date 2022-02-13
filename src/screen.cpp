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
// This is the fastest speed that worked
// I can turn it up to 72MHz, but that doesn't seem any faster :/
const uint32_t SPI_SPEED = 60000000;

// TODO: Make this behave better. Batching updates would probably really improve
// performance
Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
boolean backlight_on = false;

void set_backlight(bool turnOn) {
  if (backlight_on != turnOn) {
    digitalWrite(BACKLIGHT_PIN, turnOn ? HIGH : LOW);
    backlight_on = turnOn;
  }
}

#define BLANK_CHAR 0xA0
//                       RRRRRGGGGGGBBBBB
const uint16_t beige = 0b1100011001110001;
const uint16_t amber = 0b1111110110000000;
const uint16_t green = 0b0011011111100110;

uint16_t theColor = green; // ST77XX_WHITE
const bool rotateColors = false;

unsigned char blit[35 * 192];

void clearBuffer() {
  memset(blit, 0xff, 35 * 192);
}

void drawCharacter(unsigned char row, unsigned char col, unsigned char val) {
  if (row > 23 || col > 39) {
    return;
  }
  uint8_t* charInfo = &fontInfo[val * 8];
  uint16_t colOfs = col * 7;
  uint8_t byteOfs = colOfs / 8;
  uint8_t bitOfs = colOfs % 8;
  uint8_t mask1 = (bitOfs > 0) ? 0xFF << (8 - bitOfs) : 1;
  uint8_t mask2 = (bitOfs > 1) ? 0xFF >> (bitOfs - 1) : 0;
  for (int i = 0; i < 8; i++) {
    char init = blit[35 * (row * 8 + i) + byteOfs];
    init = (init & mask1) | (charInfo[i] >> bitOfs);
    blit[35 * (row * 8 + i) + byteOfs] = init;
    if (mask2) {
      init = blit[35 * (row * 8 + i) + 1 + byteOfs];
      init = (init & mask2) | (charInfo[i] << (8 - bitOfs));
      blit[35 * (row * 8 + i) + 1 + byteOfs] = init;
    }
  }
}

void blitBuffer() {
  tft.drawBitmap(20, 24, blit, 280, 192, ST77XX_BLACK, theColor);
}

void writeCharacter(unsigned char row, unsigned char col, unsigned char val) {
  if (row > 23 || col > 39)
    return;
  // The memory has already been updated
  // Just update the visuals
  tft.drawBitmap(20 + col * 7,
                 24 + row * 8,
                 &fontInfo[val * 8],
                 7,
                 8,
                 ST77XX_BLACK,
                 theColor);
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
  tft.setSPISpeed(SPI_SPEED);
  tft.setRotation(1);
  tft.setFont(&FreeSans12pt7b);
  tft.fillScreen(beige);
}

void clear_screen() {
  init_screen();
  for (unsigned char offs = 0; offs < 8; offs++) {
    memset(&ram[0x400 + offs * 128], BLANK_CHAR, 120); // Draw spaces everywhere
  }
  tft.fillRect(12, 16, 296, 208, ST77XX_BLACK);
}

unsigned short row_to_addr(unsigned char row) {
  // bits: 000a bcde =>  0x400 | [00cd eaba b000]
  // Due to range, if !(a && b) is true
  unsigned short ab = (row >> 3) & 3;
  unsigned short cd = (row >> 1) & 3;
  unsigned short e = row & 1;
  return 0x400 | (cd << 8) | (e << 7) | (ab << 5) | (ab << 3);
}

bufRect st = {0};

void screenScroll() {
  // move the memory while also updating the screen
  uint32_t start = micros();
  unsigned int topRow = row_to_addr(0);
  for (unsigned char row = 0; row < 23; row++) {
    unsigned int nxtRow = row_to_addr(row + 1);
    for (unsigned char col = 0; col < 40; col++) {
      unsigned char c = ram[nxtRow + col];
      if (ram[topRow + col] != c) {
        ram[topRow + col] = c;
        writeCharacter(row, col, c);
      }
    }
    topRow = nxtRow;
  }
  for (unsigned char col = 0; col < 40; col++) {
    unsigned char c = ram[topRow + col];
    if (c != BLANK_CHAR) {
      ram[topRow + col] = BLANK_CHAR;
      writeCharacter(23, col, BLANK_CHAR);
    }
  }
  uint32_t scrollTime = micros() - start;
  drawHex("%d", 140, 17, scrollTime, &st);
}

// This is reasonably well optimized. It could be further optimized by batching
// the writeCharacter calls into bigger bitmap writes...
void screenScrollBlit() {
  // move the memory while also updating the screen
  uint32_t start = micros();
  clearBuffer();
  unsigned int topRow = row_to_addr(0);
  for (unsigned char row = 0; row < 23; row++) {
    unsigned int nxtRow = row_to_addr(row + 1);
    for (unsigned char col = 0; col < 40; col++) {
      unsigned char c = ram[nxtRow + col];
      if (ram[topRow + col] != c) {
        ram[topRow + col] = c;
      }
      drawCharacter(row, col, c);
    }
    topRow = nxtRow;
  }
  memset(&ram[topRow], BLANK_CHAR, 40);
  blitBuffer();
  uint32_t scrollTime = micros() - start;
  drawHex("%d", 140, 17, scrollTime, &st);
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

void drawHex(const char* fmt,
             unsigned short x,
             unsigned short y,
             unsigned int val,
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
  if (ms - lastMs >= 3000) {
    lastMs = ms;
    // unsigned int now = micros();
    // drawHex("IPS: %d", 2, 26, total, &oldIPS);
    // unsigned int elapsed = micros() - now;
    // drawHex("draw %d", 2, 26, elapsed, &oldIPS);
    total = 0;
    if (rotateColors) {
      switch ((ms / 3000) % 3) {
        case 0:
          theColor = ST77XX_WHITE;
          break;
        case 1:
          theColor = amber;
          break;
        default:
        case 2:
          theColor = green;
          break;
      }
    }
  }
  total++;
}