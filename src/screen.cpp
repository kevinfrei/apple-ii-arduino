#include <Adafruit_GFX_Buffer.h>
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
const uint8_t BACKLIGHT_PIN = 18;
const uint8_t TFT_CS = 10;
const uint8_t TFT_DC = 20;
const uint8_t TFT_RST = 21;
// This is the fastest speed that worked
// I can turn it up to 72MHz, but that doesn't seem any faster :/
const uint32_t SPI_SPEED = 60000000;

// TODO: Make this behave better. Batching updates would probably really improve
// performance
Adafruit_ST7789 underlying_display(TFT_CS, TFT_DC, TFT_RST);
Adafruit_GFX_Buffer<Adafruit_ST7789>* tft;
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
  0b1001100000001011, // magenta
  0b0100000000111100, // dark blue
  0b1100100100011111, // purple
  0b0000010000000010, // dark green
  0b1000010000010000, // gray
  0b0010110011011111, // medium blue
  0b1010110100111111, // light blue
  0b0110001010000000, // brown
  0b1111001011100000, // orange
  0b1100011000011000, // light gray
  0b1111110000111100, // pink
  0b0001011001100001, // light green
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
  if (textMode) {
    return;
  }
  textMode = true;
  // TODO: Maybe wait an instruction or two, just to be safe?
  tft->fillRect(X_OFFSET, Y_OFFSET, 280, 192, ST77XX_BLACK);
  for (uint8_t row = 0; row < 24; row++) {
    for (uint8_t col = 0; col < 40; col++) {
      writeCharacter(row, col, ram[text_row_to_addr(row) + col], 0x20);
    }
  }
  tft->fillRect(5, 225, 15, 15, ST77XX_GREEN);
  tft->display();
}

void show_graphics() {
  if (!textMode) {
    return;
  }
  textMode = false;
  // TODO: deal with part of the screen
  if (lowRes) {
    tft->fillRect(X_OFFSET, Y_OFFSET, 280, 192, ST77XX_BLACK);
    for (uint8_t row = 0; row < 24; row++) {
      for (uint8_t col = 0; col < 40; col++) {
        writeCharacter(row, col, ram[text_row_to_addr(row) + col], 0x20);
      }
    }
  } else if (hiRes) {
    // TODO: optimize this, and make it splitmode aware
    for (uint16_t ofs = 0; ofs < 0x2000; ofs += 128) {
      for (uint16_t mem = 0; mem < 120; mem++) {
        uint16_t addr = ofs + mem + (page1 ? 0x2000 : 0x4000);
        // The inverse makes sure everything gets cleared, right?
        highWrite(addr, ram[addr], ~ram[addr]);
      }
    }
  }
  tft->fillRect(5, 225, 15, 15, ST77XX_RED);
  tft->display();
}

void show_lores() {
  lowRes = true;
  hiRes = false;
  // TODO: Maybe wait an instruction or two, just to be safe?
  tft->fillRect(X_OFFSET, Y_OFFSET, 280, (splitMode ? 160 : 192), ST77XX_BLACK);
  for (uint8_t row = 0; row < (splitMode ? 20 : 24); row++) {
    for (uint8_t col = 0; col < 40; col++) {
      writeCharacter(row, col, ram[text_row_to_addr(row) + col], 0x00);
    }
  }
  tft->fillRect(25, 225, 15, 15, ST77XX_MAGENTA);
  tft->display();
}

void show_hires() {
  lowRes = false;
  hiRes = true;
  // TODO: Make this split mode aware
  for (uint16_t ofs = 0; ofs < 0x2000; ofs += 128) {
    for (uint16_t mem = 0; mem < 120; mem++) {
      uint16_t addr = ofs + mem + (page1 ? 0x2000 : 0x4000);
      // The inverse makes sure everything gets cleared, right?
      highWrite(addr, ram[addr], ~ram[addr]);
    }
  }
  tft->fillRect(25, 225, 15, 15, ST77XX_YELLOW);
  tft->display();
}

void full_graphics() {
  splitMode = false;
  // TODO
  tft->fillRect(45, 225, 15, 15, ST77XX_WHITE);
  tft->display();
}

void split_graphics() {
  splitMode = true;
  // TODO
  tft->fillRect(45, 225, 15, 15, ST77XX_BLACK);
  tft->display();
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
#if defined(TRIM_CHARS)
    uint8_t out[8];
    uint16_t x, y, w, h;
    // TODO: Maybe memoize this function?
    // trimChar(&fontInfo[val * 8], &fontInfo[prev * 8], &out[0], &x, &y, &w,
    // &h);
    tft->drawBitmap(
      col * 7 + x, row * 8 + y, out, w, h, ST77XX_BLACK, theColor);
#else
    tft->drawBitmap(
      col * 7, row * 8, &fontInfo[val * 8], 7, 8, ST77XX_BLACK, theColor);
#endif
  } else if (lowRes) {
    if ((val & 0xF) != (prev & 0xF)) {
      tft->fillRect(
        X_OFFSET + col * 7, Y_OFFSET + row * 8, 7, 4, loClrs[val & 0xF]);
    }
    if ((val >> 4) != (prev >> 4)) {
      tft->fillRect(
        X_OFFSET + col * 7, Y_OFFSET + 4 + row * 8, 7, 4, loClrs[val >> 4]);
    }
  }
}

void init_screen() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  set_backlight(true);
#if defined(ADAFRUIT_1_14)
  tft = new Adafruit_GFX_Buffer<Adafruit_ST7789>(135, 240, underlying_display);
#elif defined(ADAFRUIT_2_0)
  tft = new Adafruit_GFX_Buffer<Adafruit_ST7789>(240, 320, underlying_display);
#else
#error Sorry, you need to pick a screen
#endif
  tft->setSPISpeed(SPI_SPEED);
  tft->setRotation(1);
  tft->setFont(&FreeSans12pt7b);
  tft->fillScreen(beige);
  tft->display();
}

void clear_screen() {
  init_screen();
  for (unsigned char offs = 0; offs < 8; offs++) {
    memset(&ram[0x400 + offs * 128], BLANK_CHAR, 120); // Draw spaces everywhere
  }
  tft->fillRect(12, 16, 296, 208, ST77XX_BLACK);
  tft->display();
}

unsigned short text_row_to_addr(unsigned char row) {
  // bits: 000a bcde =>  0x400 | [00cd eaba b000]
  // Due to range, if !(a && b) is true
  unsigned short ab = (row >> 3) & 3;
  unsigned short cd = (row >> 1) & 3;
  unsigned short e = row & 1;
  return (page1 ? 0x400 : 0x800) | (cd << 8) | (e << 7) | (ab << 5) | (ab << 3);
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
  tft->display();
}

inline uint8_t hiResAddrToRow(uint16_t addr) {
  uint16_t first = (addr >> 10) & 7;
  uint16_t second = (addr >> 4) & 0x38;
  uint16_t third = ((addr & 0x7F) / 40) << 6;
  return first | second | third;
}

inline uint16_t hiResRowToAddr(uint8_t row, bool page2 = false) {
  uint16_t third = ((row & 0xC0) >> 3) | ((row & 0xC0) >> 1);
  uint16_t second = (row & 0x38) << 4;
  uint16_t first = (row & 7) << 10;
  return first | second | third | (page2 ? 0x4000 : 0x2000);
}

bufRect oldHGR = {0};

void highWrite(unsigned short address, unsigned char val, unsigned char prv) {
  if (textMode || !hiRes || !(address & 0x2000) == page1) {
    return;
  }
  uint8_t row = hiResAddrToRow(address);
  if (row > 159 && splitMode) {
    return;
  }
  uint8_t col = (address & 0x7f) - 5 * ((row & 0xC0) >> 3);
  // Bit reverse the whole thing...
  uint8_t flip = ((val & 0x01) << 7) | ((val & 0x02) << 5) |
                 ((val & 0x04) << 3) | ((val & 0x08) << 1) |
                 ((val & 0x10) >> 1) | ((val & 0x20) >> 3) |
                 ((val & 0x40) >> 5);
  // TODO: Draw the bit on screen, but maybe 'delayed' by a couple cycles to
  // batch updates better? Not sure, but it feels like maybe batching ought to
  // be handled by the 6502 code for hires graphics (multiple memory writes are
  // slow, so folks probably optimized for this?)
  // TODO: Maybe be color aware?
  // Monochome is MUCH easier (WOZ!!! Shakes fist, while also being impressed)
  tft->drawBitmap(
    X_OFFSET + col * 7, Y_OFFSET + row, &flip, 7, 1, theColor, ST77XX_BLACK);
  tft->display();
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
  tft->display();
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
  tft->getTextBounds(buffer, x, y, &x1, &y1, &w, &h);
  tft->fillRect(oldBuf->x1, oldBuf->y1, oldBuf->w, oldBuf->h, ST77XX_BLACK);
  *oldBuf = {x1, y1, w, h};
  tft->setCursor(x, y);
  tft->print(buffer);
}

bufRect oA = {0}, oX = {0}, oY = {0}, oPC = {0}, oSR = {0}, oldIPS = {0},
        oldDT = {0};
unsigned int total = 0;
unsigned int lastMs = 0;
unsigned char oldA = 0, oldSTP = 0, oldX = 0, oldY = 0, oldSR = 0;
unsigned char oldPC = 0;
unsigned int lastCycles;
void debug_info(unsigned int ms, unsigned int cycles, int delayTime) {
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
    Serial.printf(
      "kCPS: %d\ndelay %d\n", (cycles - lastCycles) >> 10, delayTime);
    drawHex("kCPS: %d", 2, 18, (cycles - lastCycles) >> 10, &oldIPS);
    drawHex("delay %d", 2, 239, delayTime, &oldDT);
    lastMs = ms;
    lastCycles = cycles;
    total = 0;
  }
  total++;
  tft->display();
}
