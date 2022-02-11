void setflags();
void push16(unsigned short pushval);
void push8(unsigned char pushval);
unsigned short pull16();
unsigned char pull8();
void init_machine();
void loop();

extern unsigned short PC;
extern unsigned char STP, A, X, Y, SR;
