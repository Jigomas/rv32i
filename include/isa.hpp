#pragma once
#include <cstdint>

#include "types.hpp"

namespace ISA {
// OPC — bytes [6:0] of each instruction

enum Opcode : uint8_t {
    OP_LOAD     = 0b0000011,  // LB LH LW LBU LHU
    OP_MISC_MEM = 0b0001111,  // FENCE
    OP_OP_IMM   = 0b0010011,  // ADDI SLTI SLTIU XORI ORI ANDI SLLI SRLI SRAI
    OP_AUIPC    = 0b0010111,  // AUIPC
    OP_STORE    = 0b0100011,  // SB SH SW
    OP_OP       = 0b0110011,  // ADD SUB SLL SLT SLTU XOR SRL SRA OR AND  (+M)
    OP_LUI      = 0b0110111,  // LUI
    OP_BRANCH   = 0b1100011,  // BEQ BNE BLT BGE BLTU BGEU
    OP_JALR     = 0b1100111,  // JALR
    OP_JAL      = 0b1101111,  // JAL
    OP_SYSTEM   = 0b1110011,  // ECALL EBREAK
};

// FUNCT3 — arithmetic/logic (OP and OP_IMM)

enum Funct3ALU : uint8_t {
    F3_ADD_SUB = 0b000,  // ADD/SUB (by funct7) or ADDI
    F3_SLL     = 0b001,  // Shift Left Logical
    F3_SLT     = 0b010,  // Set Less Than (signed)
    F3_SLTU    = 0b011,  // Set Less Than Unsigned
    F3_XOR     = 0b100,  // XOR
    F3_SRL_SRA = 0b101,  // Shift Right Logical/Arithmetic (by funct7)
    F3_OR      = 0b110,  // OR
    F3_AND     = 0b111,  // AND
};

// FUNCT3 — M extension (multiply/divide)

enum Funct3MUL : uint8_t {
    F3_MUL    = 0b000,  // Lower 32 bits of product
    F3_MULH   = 0b001,  // Upper 32 bits (signed × signed)
    F3_MULHSU = 0b010,  // Upper 32 bits (signed × unsigned)
    F3_MULHU  = 0b011,  // Upper 32 bits (unsigned × unsigned)
    F3_DIV    = 0b100,  // Signed division
    F3_DIVU   = 0b101,  // Unsigned division
    F3_REM    = 0b110,  // Signed remainder
    F3_REMU   = 0b111,  // Unsigned remainder
};

// FUNCT7 — bits [31:25] of R-type

enum Funct7 : uint8_t {
    F7_NORMAL = 0b0000000,  // Standard operation
    F7_ALT    = 0b0100000,  // Alternative (SUB, SRA)
    F7_MEXT   = 0b0000001,  // M extension
};

// FUNCT3 — branches (BRANCH)

enum Funct3Branch : uint8_t {
    F3_BEQ  = 0b000,  // Branch Equal
    F3_BNE  = 0b001,  // Branch Not Equal
    F3_BLT  = 0b100,  // Branch Less Than (signed)
    F3_BGE  = 0b101,  // Branch Greater or Equal (signed)
    F3_BLTU = 0b110,  // Branch Less Than Unsigned
    F3_BGEU = 0b111,  // Branch Greater or Equal Unsigned
};

// FUNCT3 — load (LOAD)

enum Funct3Load : uint8_t {
    F3_LB  = 0b000,  // Load Byte with sign extension
    F3_LH  = 0b001,  // Load Halfword with sign extension
    F3_LW  = 0b010,  // Load Word
    F3_LBU = 0b100,  // Load Byte Unsigned (zero extension)
    F3_LHU = 0b101,  // Load Halfword Unsigned (zero extension)
};

// FUNCT3 — store (STORE)

enum Funct3Store : uint8_t {
    F3_SB = 0b000,  // Store Byte
    F3_SH = 0b001,  // Store Halfword
    F3_SW = 0b010,  // Store Word
};

// INSTRUCTION FIELD EXTRACTION

// Extract bits [hi:lo] from word
inline Word extractBits(Word instr, int hi, int lo) {
    int  width = hi - lo + 1;
    Word mask  = (width == 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
    return (instr >> lo) & mask;
}

// Sign-extend n-bit number to 32 bits
// Method: shift sign bit to position 31, then arithmetic shift back
inline SWord signExtend(Word val, int bits) {
    int shift = 32 - bits;
    return static_cast<SWord>(val << shift) >> shift;
}

// Field getters
inline uint8_t getOpcode(Word i) {
    return i & 0x7Fu;
}
inline uint8_t getRd(Word i) {
    return (i >> 7) & 0x1Fu;
}
inline uint8_t getFunct3(Word i) {
    return (i >> 12) & 0x07u;
}
inline uint8_t getRs1(Word i) {
    return (i >> 15) & 0x1Fu;
}
inline uint8_t getRs2(Word i) {
    return (i >> 20) & 0x1Fu;
}
inline uint8_t getFunct7(Word i) {
    return (i >> 25) & 0x7Fu;
}

// I-type: bits [31:20] → 12-bit signed immediate
inline SWord decodeImmI(Word i) {
    return signExtend(extractBits(i, 31, 20), 12);
}

// S-type: [31:25]|[11:7] → 12-bit signed immediate
inline SWord decodeImmS(Word i) {
    Word hi = extractBits(i, 31, 25);
    Word lo = extractBits(i, 11, 7);
    return signExtend((hi << 5) | lo, 12);
}

// B-type: [31][7][30:25][11:8] → 13-bit signed immediate (LSB=0)
inline SWord decodeImmB(Word i) {
    Word b12   = extractBits(i, 31, 31);
    Word b11   = extractBits(i, 7, 7);
    Word b10_5 = extractBits(i, 30, 25);
    Word b4_1  = extractBits(i, 11, 8);
    return signExtend((b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1), 13);
}

// U-type: bits [31:12] in place, [11:0] = 0
inline Word decodeImmU(Word i) {
    return i & 0xFFFFF000u;
}

// J-type: [31][19:12][20][30:21] → 21-bit signed immediate (LSB=0)
inline SWord decodeImmJ(Word i) {
    Word b20    = extractBits(i, 31, 31);
    Word b19_12 = extractBits(i, 19, 12);
    Word b11    = extractBits(i, 20, 20);
    Word b10_1  = extractBits(i, 30, 21);
    return signExtend((b20 << 20) | (b19_12 << 12) | (b11 << 11) | (b10_1 << 1), 21);
}

}  // namespace ISA