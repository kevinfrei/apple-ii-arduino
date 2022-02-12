struct bufRect {
  short x1, y1;
  unsigned short w, h;
};

void drawHex(const char* fmt,
             unsigned short x,
             unsigned short y,
             unsigned short val,
             bufRect* oldBuf);
extern unsigned char fontInfo[];
void clear_screen();
void writeCharacter(unsigned char row, unsigned char col, unsigned char val);
unsigned short row_to_addr(unsigned char row);
void screenScroll();
void screenWrite(unsigned short address, unsigned char value);
void debug_info(unsigned int ms);