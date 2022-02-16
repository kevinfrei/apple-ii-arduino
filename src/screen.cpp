#include <Adafruit_ST7789.h>
#include <stdint.h>

#include "Fonts/FreeSans12pt7b.h"
#include "cpu.h"
#include "fontdiff.h"
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
const uint16_t loClrs[16] = {
  0b0000000000000000, // black
  0b1001100000001011, // "red"
  0b0100000000111100, // 'dblue'
  0b1100100100011111, // purple
  0b0000010000000010, // dgreen
  0b1000010000010000, // gray
  0b0010110011011111, // mblue
  0b1010110100111111, // lblue
  0b0110001010000000, // brown
  0b1111001011100000, // orange
  0b1100011000011000, // lgray
  0b1111110000111100, // pink
  0b0001011001100001, // lgreen
  0b1101011010100010, // yellow
  0b0101011110110011, // aqua
  0b1111111111111111, // white
};
uint16_t theColor = green; // ST77XX_WHITE
const bool rotateColors = false;

// Stuff to deal with:
bool lowRes = false;
bool hiRes = false;
bool splitMode = false;
bool textMode = true;
bool page1 = true;

// This stuff kinda wants a trigger to actually upate the screen after things
// have "settled" so maybe wait 3-6 instructions before actually deciding to
// switch around the mode.

void show_text() {
  textMode = true;
  // TODO
}

void show_graphics() {
  textMode = false;
  // TODO
}

void show_lores() {
  lowRes = true;
  hiRes = false;
  // TODO
}

void show_hires() {
  lowRes = false;
  hiRes = true;
  // TODO
}

void full_graphics() {
  splitMode = false;
  // TODO
}

void split_graphics() {
  splitMode = true;
  // TODO
}

void show_page1() {
  page1 = true;
  // TODO
}

void show_page2() {
  page1 = false;
  // TODO
}

void writeCharacter(uint8_t row, uint8_t col, uint8_t val, uint8_t prev) {
  // The memory has already been updated. Just update the visuals.
  if ((row > 23) || (col > 39) || (splitMode ? (row < 20) : hiRes)) {
    return;
  }
  if (textMode || (splitMode && row >= 20)) {
    // Let's try to minimize the amount of bitmap updating
    if (val == prev)
      return;
    uint8_t out[8];
    uint16_t x, y, w, h;
    // TODO: Maybe memoize this function?
    trimChar(&fontInfo[val * 8], &fontInfo[prev * 8], &out[0], &x, &y, &w, &h);
    tft.drawBitmap(col * 7 + x, row * 8 + y, out, w, h, ST77XX_BLACK, theColor);
  } else if (lowRes) {
    if ((val & 0xF) != (prev & 0xF)) {
      tft.fillRect(20 + col * 7, 24 + row * 8, 7, 4, loClrs[val & 0xF]);
    }
    if ((val >> 4) != (prev >> 4)) {
      tft.fillRect(20 + col * 7, 28 + row * 8, 7, 4, loClrs[val >> 4]);
    }
  }
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

unsigned short text_row_to_addr(unsigned char row) {
  // bits: 000a bcde =>  0x400 | [00cd eaba b000]
  // Due to range, if !(a && b) is true
  unsigned short ab = (row >> 3) & 3;
  unsigned short cd = (row >> 1) & 3;
  unsigned short e = row & 1;
  return 0x400 | (cd << 8) | (e << 7) | (ab << 5) | (ab << 3);
}

// This isn't any faster than the built in routine, once I got the rendering
// optimization stuff complete: lol
// TODO: Make this work with the 0page 'window' edges at 0x20-23
void screenScroll() {
  // move the memory while also updating the screen
  unsigned int topRow = text_row_to_addr(0);
  for (unsigned char row = 0; row < 23; row++) {
    unsigned int nxtRow = text_row_to_addr(row + 1);
    for (unsigned char col = 0; col < 40; col++) {
      unsigned char c = ram[nxtRow + col];
      unsigned char prev = ram[topRow + col];
      if (prev != c) {
        ram[topRow + col] = c;
        writeCharacter(row, col, c, prev);
      }
    }
    topRow = nxtRow;
  }
  for (unsigned char col = 0; col < 40; col++) {
    unsigned char c = ram[topRow + col];
    if (c != BLANK_CHAR) {
      ram[topRow + col] = BLANK_CHAR;
      writeCharacter(23, col, BLANK_CHAR, c);
    }
  }
}

void highWrite(unsigned short address, unsigned char val, unsigned char prv) {
  /*
  0:2000
  1:2400
  2:2800
  3:2c00
  4:3000
  5:3400
  6:3800
  7:3c00
  8:2080
  ....
  15:3c80
  ....
  56:2380
  ....
  63:3f80
  64:2028
  ...
  128:2050
  ...
  191:3fd0
  */
  // LSB: Bits 11-13, then bit 7-9, then the bottom 40/80/120 (with 8 left over)
  uint8_t row =
    ((addr >> 11) & 7) | ((addr >> 4) & 0x38) + (((addr & 127) / 40) << 6);
}

void screenWrite(unsigned short address,
                 unsigned char value,
                 unsigned char prev) {
  if (value == prev) {
    return;
  }
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
  // If not bell character << This is bad for graphics mode :)
  // if (value != 0x87)
  writeCharacter(row, col, value, prev);
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