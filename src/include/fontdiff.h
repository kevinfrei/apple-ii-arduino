#pragma once
#include <stdint.h>

#include "screen.h"

// This routine jacks up a typical full screen scroll by ~30% or so. It takes
// the old and new bitmaps, then trims the bitmap to be the minimal size while
// keeping the screen correct. I can get up to about 11FPS for a full screen
// scroll with different characters
inline void trimChar(uint8_t* upd,
                     uint8_t* old,
                     uint8_t* out,
                     uint16_t* x,
                     uint16_t* y,
                     uint16_t* w,
                     uint16_t* h) {
  // Adaptive trimming code
  for (uint8_t yo = 0; yo < 8; yo++) {
    out[yo] = upd[yo] ^ old[yo];
  }
  uint8_t ys, ye;
  for (ys = 0; ys < 8; ys++) {
    if (out[ys]) {
      break;
    }
  }
  for (ye = 8; ye > ys; ye--) {
    if (out[ye - 1]) {
      break;
    }
  }
  uint8_t xs = 0, xe = 7;
  // Scan for hi-bit differences
  for (uint8_t mask = 0x80; mask != 0xFE; xs++, mask = (mask >> 1) | 0x80) {
    bool diff = false;
    for (uint8_t yo = ys; yo < ye; yo++) {
      if (out[yo] & mask) {
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
      if (out[yo] & mask) {
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
  *x = X_OFFSET + xs;
  *y = Y_OFFSET + ys;
  *w = xe - xs;
  *h = ye - ys;
}
