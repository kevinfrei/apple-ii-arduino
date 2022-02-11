extern unsigned char fontInfo[];
void clear_screen();
void writeCharacter(unsigned char row, unsigned char col, unsigned char val);
unsigned short row_to_addr(unsigned char row);
void screenScroll();
void screenWrite(unsigned short address, unsigned char value);