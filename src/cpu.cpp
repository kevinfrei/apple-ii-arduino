﻿// μ6502 - Barebones 6502 Emulator By Damian Peckett
// dpeckett.com, <damian@pecke.tt>
#include <Arduino.h>
#include <usb_serial.h>

#include "hooks.h"
#include "memory.h"
#include "screen.h"

// Address Modes
#define AD_IMP 0x01
#define AD_A 0x02
#define AD_ABS 0x03
#define AD_ABSX 0x04
#define AD_ABSY 0x05
#define AD_IMM 0x06
#define AD_IND 0x07
#define AD_INDX 0x08
#define AD_INDY 0x09
#define AD_REL 0x0A
#define AD_ZPG 0x0B
#define AD_ZPGX 0x0C
#define AD_ZPGY 0x0D

// SR Flag Modes
#define FL_NONE 0x00
#define FL_Z 0x20
#define FL_ZN 0xA0
#define FL_ZNC 0xB0
#define FL_ZC 0x30
#define FL_ALL 0xF0

// Unimplemented ops
#define UNDF 0x00

// Other constants
#define SR_FIXED_BITS 0x20
#define SR_CARRY 0x01
#define SR_ZERO 0x02
#define SR_INT 0x04
#define SR_DEC 0x08
#define SR_BRK 0x10
#define SR_OVER 0x40
#define SR_NEG 0x80

// Stack pointer base address
#define STP_BASE 0x100

// clang-format off
// high nibble SR flags, low nibble address mode
const unsigned char flags[] = {
	AD_IMP,        AD_INDX,        UNDF,         UNDF, UNDF,          FL_ZN|AD_ZPG,   FL_ZNC|AD_ZPG,  UNDF, AD_IMP,       FL_ZN|AD_IMM,   FL_ZNC|AD_A,  UNDF, UNDF,          FL_ZN|AD_ABS,   FL_ZNC|AD_ABS,  UNDF,
	AD_REL,        FL_ZN|AD_INDY,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ZPGX,  FL_ZNC|AD_ZPGX, UNDF, AD_IMP,       FL_ZN|AD_ABSY,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ABSX,  FL_ZNC|AD_ABSX, UNDF,
	AD_ABS,        FL_ZN|AD_INDX,  UNDF,         UNDF, FL_Z|AD_ZPG,   FL_ZN|AD_ZPG,   FL_ZNC|AD_ZPG,  UNDF, AD_IMP,       FL_ZN|AD_IMM,   FL_ZNC|AD_A,  UNDF, FL_Z|AD_ABS,   FL_ZN|AD_ABS,   FL_ZNC|AD_ABS,  UNDF,
	AD_REL,        FL_ZN|AD_INDY,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ZPGX,  FL_ZNC|AD_ZPGX, UNDF, AD_IMP,       FL_ZN|AD_ABSY,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ABSX,  FL_ZNC|AD_ABSX, UNDF,
	AD_IMP,        FL_ZN|AD_INDX,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ZPG,   FL_ZNC|AD_ZPG,  UNDF, AD_IMP,       FL_ZN|AD_IMM,   FL_ZNC|AD_A,  UNDF, AD_ABS,        FL_ZN|AD_ABS,   FL_ZNC|AD_ABS,  UNDF,
	AD_REL,        FL_ZN|AD_INDY,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ZPGX,  FL_ZNC|AD_ZPGX, UNDF, AD_IMP,       FL_ZN|AD_ABSY,  UNDF,         UNDF, UNDF,          FL_ZN|AD_ABSX,  FL_ZNC|AD_ABSX, UNDF,
	AD_IMP,        FL_ALL|AD_INDX, UNDF,         UNDF, UNDF,          FL_ALL|AD_ZPG,  FL_ZNC|AD_ZPG,  UNDF, FL_ZN|AD_IMP, FL_ALL|AD_IMM,  FL_ZNC|AD_A,  UNDF, AD_IND,        FL_ALL|AD_ABS,  FL_ZNC|AD_ABS,  UNDF,
	AD_REL,        FL_ALL|AD_INDY, UNDF,         UNDF, UNDF,          FL_ALL|AD_ZPGX, FL_ZNC|AD_ZPGX, UNDF, AD_IMP,       FL_ALL|AD_ABSY, UNDF,         UNDF, UNDF,          FL_ALL|AD_ABSX, FL_ZNC|AD_ABSX, UNDF,
	UNDF,          AD_INDX,        UNDF,         UNDF, AD_ZPG,        AD_ZPG,         AD_ZPG,         UNDF, FL_ZN|AD_IMP, UNDF,           FL_ZN|AD_IMP, UNDF, AD_ABS,        AD_ABS,         AD_ABS,         UNDF,
	AD_REL,        AD_INDY,        UNDF,         UNDF, AD_ZPGX,       AD_ZPGX,        AD_ZPGY,        UNDF, FL_ZN|AD_IMP, AD_ABSY,        AD_IMP,       UNDF, UNDF,          AD_ABSX,        UNDF,           UNDF,
	FL_ZN|AD_IMM,  FL_ZN|AD_INDX,  FL_ZN|AD_IMM, UNDF, FL_ZN|AD_ZPG,  FL_ZN|AD_ZPG,   FL_ZN|AD_ZPG,   UNDF, FL_ZN|AD_IMP, FL_ZN|AD_IMM,   FL_ZN|AD_IMP, UNDF, FL_ZN|AD_ABS,  FL_ZN|AD_ABS,   FL_ZN|AD_ABS,   UNDF,
	AD_REL,        FL_ZN|AD_INDY,  UNDF,         UNDF, FL_ZN|AD_ZPGX, FL_ZN|AD_ZPGX,  FL_ZN|AD_ZPGY,  UNDF, AD_IMP,       FL_ZN|AD_ABSY,  FL_ZN|AD_IMP, UNDF, FL_ZN|AD_ABSX, FL_ZN|AD_ABSX,  FL_ZN|AD_ABSY,  UNDF,
	FL_ZNC|AD_IMM, FL_ZNC|AD_INDX, UNDF,         UNDF, FL_ZNC|AD_ZPG, FL_ZNC|AD_ZPG,  FL_ZN|AD_ZPG,   UNDF, FL_ZN|AD_IMP, FL_ZNC|AD_IMM,  FL_ZN|AD_IMP, UNDF, FL_ZNC|AD_ABS, FL_ZNC|AD_ABS,	 FL_ZN|AD_ABS,   UNDF,
	AD_REL,        FL_ZNC|AD_INDY, UNDF,         UNDF, UNDF,          FL_ZNC|AD_ZPGX, FL_ZN|AD_ZPGX,  UNDF, AD_IMP,       FL_ZNC|AD_ABSY, UNDF,         UNDF, UNDF,          FL_ZNC|AD_ABSX, FL_ZN|AD_ABSX,  UNDF,
	FL_ZNC|AD_IMM, FL_ALL|AD_INDX, UNDF,         UNDF, FL_ZNC|AD_ZPG, FL_ALL|AD_ZPG,  FL_ZN|AD_ZPG,   UNDF, FL_ZN|AD_IMP, FL_ALL|AD_IMM,  AD_IMP,       UNDF, FL_ZNC|AD_ABS, FL_ALL|AD_ABS,	 FL_ZN|AD_ABS,   UNDF,
	AD_REL,        FL_ALL|AD_INDY, UNDF,         UNDF, UNDF,          FL_ALL|AD_ZPGX, FL_ZN|AD_ZPGX,  UNDF, AD_IMP,       FL_ALL|AD_ABSY, UNDF,         UNDF, UNDF,          FL_ALL|AD_ABSX, FL_ZN|AD_ABSX,  UNDF
};
// clang-format on

// CPU registers
unsigned short PC;
unsigned char STP = 0xFD, A = 0x00, X = 0x00, Y = 0x00, SR = SR_FIXED_BITS;

// Execution variables
unsigned char opflags;

// Temporary variables for flag generation
unsigned short value16, result;

inline void setflags() {
  // Mask out affected flags
  switch (opflags & 0xF0) {
    case 0xA0:
      SR &= 0x7D;
      break;
    case 0xB0:
      SR &= 0x7C;
      break;
    case 0x30:
      SR &= 0xFC;
      break;
    case 0xF0:
      SR &= 0x3C;
      break;
    case 0x20:
      SR &= 0xFD;
      break;
  }

  // Set various status flags
  if (opflags & 0x80)
    SR |= (result & 0x0080); // negative
  if (opflags & 0x20)
    SR |= (((result & 0xFF) == 0) ? 0x02 : 0); // zero
  if (opflags & 0x10)
    SR |= ((result & 0xFF00) ? 0x01 : 0); // carry
  if (opflags & 0x40)
    SR |= ((result ^ ((unsigned short)A)) & (result ^ value16) & 0x0080) >> 1;
}

// Stack functions
inline void push16(unsigned short pushval) {
  write8(STP_BASE + (STP--), (pushval >> 8) & 0xFF);
  write8(STP_BASE + (STP--), pushval & 0xFF);
}

inline void push8(unsigned char pushval) {
  write8(STP_BASE + (STP--), pushval);
}

inline unsigned char pull8() {
  return read8(STP_BASE + (++STP));
}

inline unsigned short pull16() {
  uint16_t vlo = pull8();
  uint16_t vhi = pull8();
  return vlo | (vhi << 8);
}

void init_machine() {
  // Load the reset vector
  PC = read16(0xFFFC);
  STP = 0xFD;
}

uint32_t cycleCounter = 0;
uint32_t lastus = 0;
uint32_t lastCycle = 0;

extern "C" void loop() {
  unsigned short argument_addr = 0;
  unsigned char opcode, value8;
  uint32_t us = micros();
  uint32_t cycleTime = cycleCounter - lastCycle;
  uint32_t loopTime = us - lastus;  
  if (lastCycle && cycleTime > 9 && cycleTime > loopTime) {
    // Slow us down to 1MHz
    // This doesn't quite work right.
    // What I really want is a mechanism that will let us "run fast" to
    // enable slower screen operations to catch back up with the rest of
    // the system.
    // delayMicroseconds(cycleTime - loopTime);
    lastCycle = cycleCounter;
    lastus = us;
  } else if (!lastCycle) {
    lastCycle = cycleCounter;
  }
  debug_info(millis(), cycleCounter, cycleTime - loopTime);
  // Routines for hooking apple ][ monitor routines
  PC = program_hooks(PC);

  // Get opcode / addressing mode
  opcode = read8(PC++);
  opflags = flags[opcode];

  // Addressing modes
  switch (opflags & 0x0F) {
    case AD_IMP:
    case AD_A:
      argument_addr = 0xFFFF;
      break;
    case AD_ABS:
      argument_addr = read16(PC);
      cycleCounter += 2;
      PC += 2;
      break;
    case AD_ABSX:
      argument_addr = read16(PC) + (unsigned short)X;
      cycleCounter += 2 + !!((argument_addr & 0xFF00) != (PC & 0xFF00));
      PC += 2;
      break;
    case AD_ABSY:
      argument_addr = read16(PC) + (unsigned short)Y;
      cycleCounter += 2 + !!((argument_addr & 0xFF00) != (PC & 0xFF00));
      PC += 2;
      break;
    case AD_IMM:
      argument_addr = PC++;
      break;
    case AD_IND:
      argument_addr = read16(PC);
      value16 =
        (argument_addr & 0xFF00) | ((argument_addr + 1) & 0x00FF); // Page wrap
      argument_addr = (unsigned short)read8(argument_addr) |
                      ((unsigned short)read8(value16) << 8);
      cycleCounter += 2;
      PC += 2;
      break;
    case AD_INDX:
      argument_addr = ((unsigned short)read8(PC++) + (unsigned short)X) & 0xFF;
      value16 =
        (argument_addr & 0xFF00) | ((argument_addr + 1) & 0x00FF); // Page wrap
      argument_addr = (unsigned short)read8(argument_addr) |
                      ((unsigned short)read8(value16) << 8);
      cycleCounter += 4;
      break;
    case AD_INDY:
      argument_addr = (unsigned short)read8(PC++);
      value16 =
        (argument_addr & 0xFF00) | ((argument_addr + 1) & 0x00FF); // Page wrap
      argument_addr = (unsigned short)read8(argument_addr) |
                      ((unsigned short)read8(value16) << 8);
      cycleCounter += 3 + !!((argument_addr & 0xFF00) != (PC & 0xFF00));
      argument_addr += Y;
      break;
    case AD_REL:
      argument_addr = (unsigned short)read8(PC++);
      argument_addr |= ((argument_addr & 0x80) ? 0xFF00 : 0);
      break;
    case AD_ZPG:
      argument_addr = (unsigned short)read8(PC++);
      cycleCounter++;
      break;
    case AD_ZPGX:
      argument_addr = ((unsigned short)read8(PC++) + (unsigned short)X) & 0xFF;
      cycleCounter += 2;
      break;
    case AD_ZPGY:
      argument_addr = ((unsigned short)read8(PC++) + (unsigned short)Y) & 0xFF;
      cycleCounter += 2;
      break;
  }
  cycleCounter += 2;
  // opcodes
  switch (opcode) {
    // ADC
    case 0x69:
    case 0x65:
    case 0x75:
    case 0x6D:
    case 0x7D:
    case 0x79:
    case 0x61:
    case 0x71:
      value16 = (unsigned short)read8(argument_addr);
      result = (unsigned short)A + value16 + (unsigned short)(SR & SR_CARRY);
      setflags();
      A = result & 0xFF;
      break;
    // AND
    case 0x29:
    case 0x25:
    case 0x35:
    case 0x2D:
    case 0x3D:
    case 0x39:
    case 0x21:
    case 0x31:
      result = A & read8(argument_addr);
      A = result & 0xFF;
      setflags();
      break;
    // ASL A
    case 0x0A:
      value16 = (unsigned short)A;
      result = value16 << 1;
      setflags();
      A = result & 0xFF;
      break;
    // ASL
    case 0x06:
    case 0x16:
    case 0x0E:
    case 0x1E:
      value16 = read8(argument_addr);
      result = value16 << 1;
      setflags();
      cycleCounter += 2;
      write8(argument_addr, result & 0xFF);
      break;
    // BCC
    case 0x90:
      if (!(SR & SR_CARRY))
        PC += argument_addr;
      break;
    // BCS
    case 0xB0:
      if ((SR & SR_CARRY))
        PC += argument_addr;
      break;
    // BEQ
    case 0xF0:
      if ((SR & SR_ZERO))
        PC += argument_addr;
      break;
    // BNE
    case 0xD0:
      if (!(SR & SR_ZERO))
        PC += argument_addr;
      break;
    // BIT
    case 0x24:
    case 0x2C:
      value8 = read8(argument_addr);
      result = A & value8;
      setflags();
      SR = (SR & 0x3F) | (value8 & 0xC0);
      break;
    // BMI
    case 0x30:
      if ((SR & SR_NEG))
        PC += argument_addr;
      break;
    // BPL
    case 0x10:
      if (!(SR & SR_NEG))
        PC += argument_addr;
      break;
    // BRK
    case 0x00:
      PC++;
      push16(PC);
      push8(SR | SR_BRK);
      SR |= SR_INT;
      PC = read16(0xFFFE);
      break;
    // BVC
    case 0x50:
      if (!(SR & SR_OVER))
        PC += argument_addr;
      break;
    // BVS
    case 0x70:
      if (SR & SR_OVER)
        PC += argument_addr;
      break;
    // CLC
    case 0x18:
      SR &= 0xFE;
      break;
    // CLD
    case 0xD8:
      SR &= 0xF7;
      break;
    // CLI
    case 0x58:
      SR &= 0xFB;
      break;
    // CLV
    case 0xB8:
      SR &= 0xBF;
      break;
    // CMP
    case 0xC9:
    case 0xC5:
    case 0xD5:
    case 0xCD:
    case 0xDD:
    case 0xD9:
    case 0xC1:
    case 0xD1:
      value16 = ((unsigned short)read8(argument_addr)) ^ 0x00FF;
      result = (unsigned short)A + value16 + (unsigned short)1;
      setflags();
      break;
    // CPX
    case 0xE0:
    case 0xE4:
    case 0xEC:
      value16 = ((unsigned short)read8(argument_addr)) ^ 0x00FF;
      result = (unsigned short)X + value16 + (unsigned short)1;
      setflags();
      break;
    // CPY
    case 0xC0:
    case 0xC4:
    case 0xCC:
      value16 = ((unsigned short)read8(argument_addr)) ^ 0x00FF;
      result = (unsigned short)Y + value16 + (unsigned short)1;
      setflags();
      break;
    // DEC
    case 0xC6:
    case 0xD6:
    case 0xCE:
    case 0xDE:
      cycleCounter += 2;
      value16 = (unsigned short)read8(argument_addr);
      result = value16 - 1;
      setflags();
      write8(argument_addr, result & 0xFF);
      break;
    // DEX
    case 0xCA:
      result = --X;
      setflags();
      break;
    // DEY
    case 0x88:
      result = --Y;
      setflags();
      break;
    // EOR
    case 0x49:
    case 0x45:
    case 0x55:
    case 0x4D:
    case 0x5D:
    case 0x59:
    case 0x41:
    case 0x51:
      value8 = read8(argument_addr);
      result = A ^ value8;
      setflags();
      A = result & 0xFF;
      break;
    // INC
    case 0xE6:
    case 0xF6:
    case 0xEE:
    case 0xFE:
      cycleCounter += 2;
      value16 = (unsigned short)read8(argument_addr);
      result = value16 + 1;
      setflags();
      write8(argument_addr, result & 0xFF);
      break;
    // INX
    case 0xE8:
      result = ++X;
      setflags();
      break;
    // INY
    case 0xC8:
      result = ++Y;
      setflags();
      break;
    // JMP
    case 0x4C:
    case 0x6C:
      PC = argument_addr;
      cycleCounter -= 1; // I think this is right...
      break;
    // JSR
    case 0x20:
      push16(PC - 1);
      PC = argument_addr;
      cycleCounter += 2;
      break;
    // LDA
    case 0xA9:
    case 0xA5:
    case 0xB5:
    case 0xAD:
    case 0xBD:
    case 0xB9:
    case 0xA1:
    case 0xB1:
      A = read8(argument_addr);
      result = A;
      setflags();
      break;
    // LDX
    case 0xA2:
    case 0xA6:
    case 0xB6:
    case 0xAE:
    case 0xBE:
      X = read8(argument_addr);
      result = X;
      setflags();
      break;
    // LDY
    case 0xA0:
    case 0xA4:
    case 0xB4:
    case 0xAC:
    case 0xBC:
      Y = read8(argument_addr);
      result = Y;
      setflags();
      break;
    // LSR A
    case 0x4A:
      value8 = A;
      result = value8 >> 1;
      result |= (value8 & 0x1) ? 0x8000 : 0;
      setflags();
      A = result & 0xFF;
      break;
    // LSR
    case 0x46:
    case 0x56:
    case 0x4E:
    case 0x5E:
      value8 = read8(argument_addr);
      result = value8 >> 1;
      result |= (value8 & 0x1) ? 0x8000 : 0;
      setflags();
      cycleCounter += 2;
      write8(argument_addr, result & 0xFF);
      break;
    // NOP
    case 0xEA:
      break;
    // ORA
    case 0x09:
    case 0x05:
    case 0x15:
    case 0x0D:
    case 0x1D:
    case 0x19:
    case 0x01:
    case 0x11:
      value8 = read8(argument_addr);
      result = A | value8;
      setflags();
      A = result & 0xFF;
      break;
    // PHA
    case 0x48:
      push8(A);
      cycleCounter += 1;
      break;
    // PHP
    case 0x08:
      push8(SR | SR_BRK);
      cycleCounter += 1;
      break;
    // PLA
    case 0x68:
      result = pull8();
      setflags();
      A = result;
      cycleCounter += 2;
      break;
      // PLP
    case 0x28:
      SR = pull8() | SR_FIXED_BITS;
      cycleCounter += 2;
      break;
    // ROL A
    case 0x2A:
      value16 = (unsigned short)A;
      result = (value16 << 1) | (SR & SR_CARRY);
      setflags();
      A = result & 0xFF;
      break;
    // ROL
    case 0x26:
    case 0x36:
    case 0x2E:
    case 0x3E:
      value16 = (unsigned short)read8(argument_addr);
      result = (value16 << 1) | (SR & SR_CARRY);
      setflags();
      cycleCounter += 2;
      write8(argument_addr, result & 0xFF);
      break;
    // ROR A
    case 0x6A:
      value16 = (unsigned short)A;
      result = (value16 >> 1) | ((SR & SR_CARRY) << 7);
      result |= (value16 & 0x1) ? 0x8000 : 0;
      setflags();
      A = result & 0xFF;
      break;
    // ROR
    case 0x66:
    case 0x76:
    case 0x6E:
    case 0x7E:
      value16 = (unsigned short)read8(argument_addr);
      result = (value16 >> 1) | ((SR & SR_CARRY) << 7);
      result |= (value16 & 0x1) ? 0x8000 : 0;
      setflags();
      cycleCounter += 1;
      write8(argument_addr, result & 0xFF);
      break;
    // RTI
    case 0x40:
      SR = pull8();
      PC = pull16();
      cycleCounter += 4;
      break;
    // RTS
    case 0x60:
      PC = pull16() + 1;
      cycleCounter += 4;
      break;
    // SBC
    case 0xE9:
    case 0xE5:
    case 0xF5:
    case 0xED:
    case 0xFD:
    case 0xF9:
    case 0xE1:
    case 0xF1:
      value16 = ((unsigned short)read8(argument_addr)) ^ 0x00FF;
      result = (unsigned short)A + value16 + (unsigned short)(SR & SR_CARRY);
      setflags();
      A = result & 0xFF;
      break;
    // SEC
    case 0x38:
      SR |= SR_CARRY;
      break;
    // SED
    case 0xF8:
      SR |= SR_DEC;
      break;
    // SEI
    case 0x78:
      SR |= SR_INT;
      break;
    // STA
    case 0x85:
    case 0x95:
    case 0x8D:
    case 0x9D:
    case 0x99:
    case 0x81:
    case 0x91:
      // There might need to be a minor adjustment for page crossed stuff for
      // the clock counter
      write8(argument_addr, A);
      break;
    // STX
    case 0x86:
    case 0x96:
    case 0x8E:
      write8(argument_addr, X);
      break;
    // STY
    case 0x84:
    case 0x94:
    case 0x8C:
      write8(argument_addr, Y);
      break;
    // TAX
    case 0xAA:
      X = A;
      result = A;
      setflags();
      break;
    // TAY
    case 0xA8:
      Y = A;
      result = A;
      setflags();
      break;
    // TSX
    case 0xBA:
      X = STP;
      result = STP;
      setflags();
      break;
    // TXA
    case 0x8A:
      A = X;
      result = X;
      setflags();
      break;
    // TXS
    case 0x9A:
      STP = X;
      result = X;
      setflags();
      break;
    // TYA
    case 0x98:
      A = Y;
      result = Y;
      setflags();
      break;
  }
}
