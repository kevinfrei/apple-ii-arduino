// Extract row, column addressing from woz's interlacing mode
static unsigned char g_row = 0, g_col = 0;
void decodeScreenAddress(unsigned short address) {
  unsigned char block_of_lines = (address >> 7) - 0x08;
  unsigned char block_offset =
    (address & 0x00FF) - ((block_of_lines & 0x01) ? 0x80 : 0x00);
  if (block_offset < 0x28) {
    g_row = block_of_lines;
    g_col = block_offset;
  } else if (block_offset < 0x50) {
    g_row = block_of_lines + 8;
    g_col = block_offset - 0x28;
  } else {
    g_row = block_of_lines + 16;
    g_col = block_offset - 0x50;
  }
}

/*
unsigned char screenRead(unsigned short address) {
  // Find our row/column
  decodeScreenAddress(address);
  return ghetto VGA To Apple(read Character(g_row, g_col));
}
*/

void screenWrite(unsigned short address, unsigned char value) {
  // Find our row/column
  decodeScreenAddress(address);
  // If not bell character
  if (value != 0x87)
    writeCharacter(g_row, g_col, value);
}
