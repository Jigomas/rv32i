#pragma once
#include <cstdint>
#include <vector>

#include "isa.hpp"
#include "memory.hpp"
#include "types.hpp"

namespace InstrBuilder {

using namespace ISA;

// R-type: rd = rs1 OP rs2  (funct7 distinguishes ADD/SUB, SRL/SRA, base/M extensions)
inline Word
R(uint8_t funct7, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    return (Word(funct7) << 25) | (Word(rs2) << 20) | (Word(rs1) << 15) | (Word(funct3) << 12) |
           (Word(rd) << 7) | Word(opcode);
}

// I-type: rd = rs1 OP imm  (load, addi, jalr, ...)
inline Word I(int16_t imm, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    Word imm12 = static_cast<Word>(imm) & 0xFFFu;
    return (imm12 << 20) | (Word(rs1) << 15) | (Word(funct3) << 12) | (Word(rd) << 7) |
           Word(opcode);
}

// S-type: mem[rs1+imm] = rs2  (sb, sh, sw)
inline Word S(int16_t imm, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t opcode) {
    Word imm12 = static_cast<Word>(imm) & 0xFFFu;
    return (((imm12 >> 5) & 0x7Fu) << 25) | (Word(rs2) << 20) | (Word(rs1) << 15) |
           (Word(funct3) << 12) | ((imm12 & 0x1Fu) << 7) | Word(opcode);
}

// B-type: if (rs1 OP rs2) PC += offset  (beq, bne, blt, ...)
// offset — in bytes, must be even
inline Word B(int16_t offset, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t opcode) {
    Word o     = static_cast<Word>(offset) & 0x1FFEu;
    Word b12   = (o >> 12) & 1u;
    Word b11   = (o >> 11) & 1u;
    Word b10_5 = (o >> 5) & 0x3Fu;
    Word b4_1  = (o >> 1) & 0xFu;
    return (b12 << 31) | (b10_5 << 25) | (Word(rs2) << 20) | (Word(rs1) << 15) |
           (Word(funct3) << 12) | (b4_1 << 8) | (b11 << 7) | Word(opcode);
}

// J-type: rd = PC+4 ; PC += offset  (jal)
inline Word J(int32_t offset, uint8_t rd, uint8_t opcode) {
    Word o      = static_cast<Word>(offset) & 0x1FFFFEu;
    Word b20    = (o >> 20) & 1u;
    Word b19_12 = (o >> 12) & 0xFFu;
    Word b11    = (o >> 11) & 1u;
    Word b10_1  = (o >> 1) & 0x3FFu;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | (Word(rd) << 7) |
           Word(opcode);
}

// Pseudo-instructions — shortcuts for most common operations

// ADDI rd, rs1, imm
inline Word ADDI(uint8_t rd, uint8_t rs1, int16_t imm) {
    return I(imm, rs1, F3_ADD_SUB, rd, OP_OP_IMM);
}

// ADD rd, rs1, rs2
inline Word ADD(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_NORMAL, rs2, rs1, F3_ADD_SUB, rd, OP_OP);
}

// SUB rd, rs1, rs2
inline Word SUB(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_ALT, rs2, rs1, F3_ADD_SUB, rd, OP_OP);
}

// MUL rd, rs1, rs2  (M extension)
inline Word MUL(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_MEXT, rs2, rs1, F3_MUL, rd, OP_OP);
}

// LW rd, offset(rs1)
inline Word LW(uint8_t rd, uint8_t rs1, int16_t offset) {
    return I(offset, rs1, F3_LW, rd, OP_LOAD);
}

// SW rs2, offset(rs1)
inline Word SW(uint8_t rs2, uint8_t rs1, int16_t offset) {
    return S(offset, rs2, rs1, F3_SW, OP_STORE);
}

// JAL rd, offset
inline Word JAL(uint8_t rd, int32_t offset) {
    return J(offset, rd, OP_JAL);
}

// Program termination marker
inline Word HALT() {
    return 0x00000000u;
}

// Load instruction vector into memory starting at base address
inline void loadProgram(Memory& mem, const std::vector<Word>& prog, Addr base = 0) {
    std::vector<uint8_t> bytes;
    bytes.reserve(prog.size() * 4);
    for (Word w : prog) {
        bytes.push_back(w & 0xFFu);
        bytes.push_back((w >> 8) & 0xFFu);
        bytes.push_back((w >> 16) & 0xFFu);
        bytes.push_back((w >> 24) & 0xFFu);
    }
    mem.loadProgram(bytes, base);
}

}  // namespace InstrBuilder