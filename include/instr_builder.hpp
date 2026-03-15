#pragma once
#include <cassert>
#include <cstdint>
#include <vector>

#include "isa.hpp"
#include "memory_model.hpp"
#include "types.hpp"

namespace InstrBuilder {

using namespace ISA;

// R-type: rd = rs1 OP rs2
inline Word
R(uint8_t funct7, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    assert(funct7 < 128 && rd < 32 && rs1 < 32 && rs2 < 32 && funct3 < 8 &&
           "R-type: field out of range");
    return (Word(funct7) << 25) | (Word(rs2) << 20) | (Word(rs1) << 15) | (Word(funct3) << 12) |
           (Word(rd) << 7) | Word(opcode);
}

// I-type: rd = rs1 OP imm  (load, addi, jalr, ...)
inline Word I(int16_t imm, uint8_t rs1, uint8_t funct3, uint8_t rd, uint8_t opcode) {
    assert(rd < 32 && rs1 < 32 && funct3 < 8 && "I-type: register/funct3 out of range");
    assert(imm >= -2048 && imm <= 2047 && "I-type: immediate out of [-2048, 2047]");
    Word imm12 = static_cast<Word>(imm) & 0b111111111111u;
    return (imm12 << 20) | (Word(rs1) << 15) | (Word(funct3) << 12) | (Word(rd) << 7) |
           Word(opcode);
}

// S-type: mem[rs1+imm] = rs2
inline Word S(int16_t imm, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t opcode) {
    assert(rs1 < 32 && rs2 < 32 && funct3 < 8 && "S-type: register/funct3 out of range");
    assert(imm >= -2048 && imm <= 2047 && "S-type: immediate out of [-2048, 2047]");
    Word imm12 = static_cast<Word>(imm) & 0b111111111111u;
    return (((imm12 >> 5) & 0b1111111u) << 25) | (Word(rs2) << 20) | (Word(rs1) << 15) |
           (Word(funct3) << 12) | ((imm12 & 0b11111u) << 7) | Word(opcode);
}

// B-type: if (rs1 OP rs2) PC += offset  (offset must be even, non-zero)
inline Word B(int16_t offset, uint8_t rs2, uint8_t rs1, uint8_t funct3, uint8_t opcode) {
    assert(rs1 < 32 && rs2 < 32 && funct3 < 8 && "B-type: register/funct3 out of range");
    assert((offset & 1) == 0 && "B-type: offset must be 2-byte aligned");
    assert(offset >= -4096 && offset <= 4094 && "B-type: offset out of [-4096, 4094]");
    Word o     = static_cast<Word>(offset) & 0x1FFEu;
    Word b12   = (o >> 12) & 1u;
    Word b11   = (o >> 11) & 1u;
    Word b10_5 = (o >> 5) & 0b111111u;
    Word b4_1  = (o >> 1) & 0b1111u;
    return (b12 << 31) | (b10_5 << 25) | (Word(rs2) << 20) | (Word(rs1) << 15) |
           (Word(funct3) << 12) | (b4_1 << 8) | (b11 << 7) | Word(opcode);
}

// U-type: LUI, AUIPC
inline Word U(int32_t imm, uint8_t rd, uint8_t opcode) {
    assert(rd < 32 && "U-type: rd out of range");
    return (static_cast<Word>(imm) & 0xFFFFF000u) | (Word(rd) << 7) | Word(opcode);
}

// J-type: rd = PC+4 ; PC += offset
inline Word J(int32_t offset, uint8_t rd, uint8_t opcode) {
    assert(rd < 32 && "J-type: rd out of range");
    assert((offset & 1) == 0 && "J-type: offset must be 2-byte aligned");
    Word o      = static_cast<Word>(offset) & 0x1FFFFEu;
    Word b20    = (o >> 20) & 1u;
    Word b19_12 = (o >> 12) & 0xFFu;
    Word b11    = (o >> 11) & 1u;
    Word b10_1  = (o >> 1) & 0b1111111111u;
    return (b20 << 31) | (b10_1 << 21) | (b11 << 20) | (b19_12 << 12) | (Word(rd) << 7) |
           Word(opcode);
}

// Pseudo-instructions
inline Word ADDI(uint8_t rd, uint8_t rs1, int16_t imm) {
    return I(imm, rs1, F3_ADD_SUB, rd, OP_OP_IMM);
}
inline Word ADD(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_NORMAL, rs2, rs1, F3_ADD_SUB, rd, OP_OP);
}
inline Word SUB(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_ALT, rs2, rs1, F3_ADD_SUB, rd, OP_OP);
}
inline Word MUL(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_MEXT, rs2, rs1, F3_MUL, rd, OP_OP);
}
inline Word LW(uint8_t rd, uint8_t rs1, int16_t offset) {
    return I(offset, rs1, F3_LW, rd, OP_LOAD);
}
inline Word SW(uint8_t rs2, uint8_t rs1, int16_t offset) {
    return S(offset, rs2, rs1, F3_SW, OP_STORE);
}
inline Word JAL(uint8_t rd, int32_t offset) {
    return J(offset, rd, OP_JAL);
}
inline Word HALT() {
    return 0x00000000u;
}

// Load instruction vector into MemoryModel — lvalue overload (copies prog)
// Note: Word (uint32_t) is a trivially-copyable type; const& is used for generality.
template <int XLEN = 32>
inline void loadProgram(MemoryModel<XLEN>&              mem,
                        const std::vector<Word>&        prog,
                        typename XlenTraits<XLEN>::Addr base = 0) {
    std::vector<uint8_t> bytes;
    bytes.reserve(prog.size() * 4);
    for (const Word w : prog) {
        bytes.push_back(static_cast<uint8_t>(w & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((w >> 16) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((w >> 24) & 0xFFu));
    }
    mem.loadProgram(std::move(bytes), base);  // move bytes into MemoryModel (rvalue)
}

// rvalue overload — accepts temporaries without copy
template <int XLEN = 32>
inline void loadProgram(MemoryModel<XLEN>&              mem,
                        std::vector<Word>&&             prog,
                        typename XlenTraits<XLEN>::Addr base = 0) {
    loadProgram<XLEN>(mem, static_cast<const std::vector<Word>&>(prog), base);
}

}  // namespace InstrBuilder