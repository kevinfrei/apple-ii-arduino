void writeCharacter(unsigned char row, unsigned char col, unsigned char val) {
  // The memory has already been updated
  // TODO: This is just to update the visuals
}

void clearScreen() {
  for (unsigned char offs = 0; offs < 8; offs++) {
    memset(&ram[0x400 + offs * 128], 0, 120);
  }
  // TODO: Update the visuals
}

unsigned short row_to_addr(unsigned char row) {
  // bits: 000a bcde =>  0x400 [0100 0000 0000] | [01cd eaba b000]
  // Due to range, if !(a && b) is true
  unsigned short ab = (row >> 3) & 3;
  unsigned short cd = (row >> 1) & 3;
  unsigned short e = row & 1;
  return 0x400 | (cd << 8) | (e << 7) | (ab << 5) | (ab << 3);
}

void screenScroll() {
  // move the memory while also updating the screen
  for (unsigned char row = 0; row < 24; row++) {
    memcpy(&ram[row_to_addr(row)], &ram[row_to_addr(row+1), 40);
  }
  memset(&ram[row_to_addr(23)], 0, 40);
  // TODO: Update the visuals
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
