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

// This routine jacks up a typical full screen scroll by ~30% or so. It takes
// the old and new bitmaps, then trims the bitmap to be the minimal size while
// keeping the screen correct. I can get up to about 11FPS for a full screen
// scroll with different characters
inline bool trimChar(uint8_t* upd, uint8_t* old, uint8_t* out) {
  // For speed, just check for the outer edges, and the bottom row
  if ((upd[0] & 0x83) == (old[0] & 0x83) &&
      (upd[1] & 0x83) == (old[1] & 0x83) &&
      (upd[2] & 0x83) == (old[2] & 0x83) &&
      (upd[3] & 0x83) == (old[3] & 0x83) &&
      (upd[4] & 0x83) == (old[4] & 0x83) &&
      (upd[5] & 0x83) == (old[5] & 0x83) &&
      (upd[6] & 0x83) == (old[6] & 0x83) && (upd[7] == old[7])) {
    out[0] = upd[0] << 1;
    out[1] = upd[1] << 1;
    out[2] = upd[2] << 1;
    out[3] = upd[3] << 1;
    out[4] = upd[4] << 1;
    out[5] = upd[5] << 1;
    out[6] = upd[6] << 1;
    return true;
  }
  return false;
  /* Adaptive trimming code, fully tested, below:
  uint8_t tmp[8];
  for (uint8_t yo = 0; yo < 8; yo++) {
    tmp[yo] = upd[yo] ^ old[yo];
  }
  uint8_t ys, ye;
  for (ys = 0; ys < 8; ys++) {
    if (tmp[ys]) {
      break;
    }
  }
  for (ye = 8; ye > ys; ye--) {
    if (tmp[ye - 1]) {
      break;
    }
  }
  uint8_t xs = 0, xe = 7;
  // Scan for hi-bit differences
  for (uint8_t mask = 0x80; mask != 0xFE; xs++, mask = (mask >> 1) | 0x80) {
    bool diff = false;
    for (uint8_t yo = ys; yo < ye; yo++) {
      if (tmp[yo] & mask) {
        // We found a difference
        diff = true;
        break;
      }
    }
    if (diff) {
      break;
    }
  }
  xe = 7;
  for (uint8_t mask = 0x2; mask != 0xFE; xe--, mask = (mask << 1) | 2) {
    bool diff = false;
    for (uint8_t yo = ys; yo < ye; yo++) {
      if (tmp[yo] & mask) {
        // We found a difference
        diff = true;
        break;
      }
    }
    if (diff) {
      break;
    }
  }
  // Copy the changed bits into the new bitmap
  for (uint8_t yo = ys; yo < ye; yo++) {
    out[yo - ys] = upd[yo] << xs;
  }
  *x = 20 + xs;
  *y = 24 + ys;
  *w = xe - xs;
  *h = ye - ys;
 */
}

void writeCharacter(uint8_t row, uint8_t col, uint8_t val, uint8_t prev) {
  // The memory has already been updated. Just update the visuals.
  if ((row > 23) || (col > 39) || (splitMode ? (row < 20) : hiRes)) {
    return;
  }
  if (textMode || (splitMode && row >= 20)) {
    // Let's try to minimize the amount of bitmap updating
    uint8_t out[7];
    bool trimmed = trimChar(&fontInfo[val * 8], &fontInfo[prev * 8], &out[0]);
    if (trimmed) {
      tft.drawBitmap(
        col * 7 + 21, row * 8 + 24, out, 5, 7, ST77XX_BLACK, theColor);
    } else {
      tft.drawBitmap(col * 7 + 20,
                     row * 8 + 24,
                     &fontInfo[val * 8],
                     7,
                     8,
                     ST77XX_BLACK,
                     theColor);
    }
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

unsigned short row_to_addr(unsigned char row) {
  // bits: 000a bcde =>  0x400 | [00cd eaba b000]
  // Due to range, if !(a && b) is true
  unsigned short ab = (row >> 3) & 3;
  unsigned short cd = (row >> 1) & 3;
  unsigned short e = row & 1;
  return 0x400 | (cd << 8) | (e << 7) | (ab << 5) | (ab << 3);
}

bufRect st = {0};

// This isn't any faster than the built in routine, once I got the rendering
// optimization stuff complete: lol
// TODO: Make this work with the 0page 'window' edges at 0x20-23
void screenScroll() {
  // move the memory while also updating the screen
  uint32_t start = micros();
  unsigned int topRow = row_to_addr(0);
  for (unsigned char row = 0; row < 23; row++) {
    unsigned int nxtRow = row_to_addr(row + 1);
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
  uint32_t scrollTime = micros() - start;
  drawHex("%d", 140, 17, scrollTime, &st);
}

void screenWrite(unsigned short address,
                 unsigned char value,
                 unsigned char prev) {
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