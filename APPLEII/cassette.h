void cassette_header(unsigned short periods);
void cassette_write_byte(unsigned char val);
void cassette_write_block(unsigned short A1, unsigned short A2);
boolean cassette_read_state();
short cassette_read_transition();
boolean cassette_read_block(unsigned short A1, unsigned short A2);
void cassette_begin();
