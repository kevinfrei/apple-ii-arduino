struct bufRect {
  short x1, y1;
  unsigned short w, h;
};

void drawHex(const char* fmt,
             unsigned short x,
             unsigned short y,
             unsigned int val,
             bufRect* oldBuf);
extern unsigned char fontInfo[];
void clear_screen();
void writeCharacter(unsigned char row, unsigned char col, unsigned char val, unsigned char prev);
unsigned short row_to_addr(unsigned char row);
void screenScroll();
void screenWrite(unsigned short address, unsigned char value, unsigned char prev);
void debug_info(unsigned int ms);

void show_text();
void show_graphics();
void show_lores();
void show_hires();
void full_graphics();
void split_graphics();