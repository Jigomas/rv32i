#pragma once
#include <cassert>
#include <cstdint>

#include "types.hpp"

namespace ISA {

enum Opcode : uint8_t {
    OP_LOAD     = 0b0000011,
    OP_MISC_MEM = 0b0001111,
    OP_OP_IMM   = 0b0010011,
    OP_AUIPC    = 0b0010111,
    OP_STORE    = 0b0100011,
    OP_OP       = 0b0110011,
    OP_LUI      = 0b0110111,
    OP_BRANCH   = 0b1100011,
    OP_JALR     = 0b1100111,
    OP_JAL      = 0b1101111,
    OP_SYSTEM   = 0b1110011,
};

enum Funct3ALU : uint8_t {
    F3_ADD_SUB = 0b000,
    F3_SLL     = 0b001,
    F3_SLT     = 0b010,
    F3_SLTU    = 0b011,
    F3_XOR     = 0b100,
    F3_SRL_SRA = 0b101,
    F3_OR      = 0b110,
    F3_AND     = 0b111,
};

enum Funct3MUL : uint8_t {
    F3_MUL    = 0b000,
    F3_MULH   = 0b001,
    F3_MULHSU = 0b010,
    F3_MULHU  = 0b011,
    F3_DIV    = 0b100,
    F3_DIVU   = 0b101,
    F3_REM    = 0b110,
    F3_REMU   = 0b111,
};

enum Funct7 : uint8_t {
    F7_NORMAL = 0b0000000,
    F7_ALT    = 0b0100000,
    F7_MEXT   = 0b0000001,
};

enum Funct3Branch : uint8_t {
    F3_BEQ  = 0b000,
    F3_BNE  = 0b001,
    F3_BLT  = 0b100,
    F3_BGE  = 0b101,
    F3_BLTU = 0b110,
    F3_BGEU = 0b111,
};

enum Funct3Load : uint8_t {
    F3_LB  = 0b000,
    F3_LH  = 0b001,
    F3_LW  = 0b010,
    F3_LBU = 0b100,
    F3_LHU = 0b101,
};

enum Funct3Store : uint8_t {
    F3_SB = 0b000,
    F3_SH = 0b001,
    F3_SW = 0b010,
};

inline Word extractBits(Word instr, int hi, int lo) {
    assert(hi >= lo && "extractBits: hi must be >= lo");
    assert(hi <= 31 && lo >= 0 && "extractBits: bit indices out of [0,31]");
    int  width = hi - lo + 1;
    Word mask  = (width == 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
    return (instr >> lo) & mask;
}

inline SWord signExtend(Word val, int bits) {
    assert(bits >= 1 && bits <= 32 && "signExtend: bits must be in [1,32]");
    int shift = 32 - bits;
    return static_cast<SWord>(val << shift) >> shift;
}

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

inline SWord decodeImmI(Word i) {
    return signExtend(extractBits(i, 31, 20), 12);
}

inline SWord decodeImmS(Word i) {
    Word hi = extractBits(i, 31, 25);
    Word lo = extractBits(i, 11, 7);
    return signExtend((hi << 5) | lo, 12);
}

inline SWord decodeImmB(Word i) {
    Word b12   = extractBits(i, 31, 31);
    Word b11   = extractBits(i, 7, 7);
    Word b10_5 = extractBits(i, 30, 25);
    Word b4_1  = extractBits(i, 11, 8);
    return signExtend((b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1), 13);
}

inline Word decodeImmU(Word i) {
    return i & 0xFFFFF000u;
}

inline SWord decodeImmJ(Word i) {
    Word b20    = extractBits(i, 31, 31);
    Word b19_12 = extractBits(i, 19, 12);
    Word b11    = extractBits(i, 20, 20);
    Word b10_1  = extractBits(i, 30, 21);
    return signExtend((b20 << 20) | (b19_12 << 12) | (b11 << 11) | (b10_1 << 1), 21);
}

}  // namespace ISA