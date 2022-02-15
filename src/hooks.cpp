#include <Arduino.h>

#include "cassette.h"
#include "cpu.h"
#include "memory.h"
#include "screen.h"

bufRect st = {0};
uint32_t lastTime = 0;

// Hook some routines for the apple II monitor program
// to trick the apple code into working on the simulator
// I could patch the ROM, but this is literally ~15 cycles on failure
unsigned short program_hooks(unsigned short addr) {
  // hook screen scroll, monitor command
  switch (addr) {
    // The scroll hook is only *slightly* faster than running the 6502
    // interpreter, but it doesn't yet handle the scroll frame values in ZP
    // properly
    case 0xFC70: {
      // uint32_t scrollTime = micros() - lastTime;
      // if (scrollTime < 999999) {
      // drawHex("%d", 140, 17, scrollTime, &st);
      // }
      // lastTime = micros();
      break;
      screenScroll();
      // This is the wrong return address depending on the ROM.
      // Just pull the retaddr from the stack? That seems safer...
      return 0xFC95;
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
      return 0xFEF5;
    }
    // hook cassette read command
    case 0xFEFD: {
      // Read Data Block
      bool success = cassette_read_block(read16(0x3C), read16(0x3E));
      // Emulate counter behaviour
      write16(0x003C, read16(0x3E));
      if (success)
        return 0xFF3A;
      else
        return 0xFF2D;
    }
  }
  return addr;
}
