#include "cassette.h"
#include "cpu.h"
#include "memory.h"
#include "screen.h"

// Hook routines for the apple II monitor program
// Used to trick the apple code into working on this hardware
// Ideally should patch the ROM itself, will do in future.
void program_hooks(unsigned short addr) {
  // hook screen scroll, monitor command
  switch (addr) {
    case 0xFC70: {
      screenScroll();
      PC = 0xFC95;
      return;
    }
    // hook cassette write commnand
    case 0xFECD:
    case 0xFECF: {
      // Header length
      cassette_header((addr == 0xFECD) ? 64 : A);
      // Write Data Block
      cassette_write_block(read16(0x3C), read16(0x3E));
      // Emulate counter behaviour
      write16(0x003C, read16(0x3E));
      PC = 0xFEF5;
      return;
    }
    // hook cassette read command
    case 0xFEFD: {
      // Read Data Block
      bool success = cassette_read_block(read16(0x3C), read16(0x3E));
      // Emulate counter behaviour
      write16(0x003C, read16(0x3E));
      if (success)
        PC = 0xFF3A;
      else
        PC = 0xFF2D;
      return;
    }
  }
}
