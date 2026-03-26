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
inline Word LUI(uint8_t rd, int32_t imm20) {
    return U(imm20 << 12, rd, OP_LUI);
}
inline Word AUIPC(uint8_t rd, int32_t imm20) {
    return U(imm20 << 12, rd, OP_AUIPC);
}
inline Word SLLI(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return R(F7_NORMAL, shamt, rs1, F3_SLL, rd, OP_OP_IMM);
}
inline Word SRLI(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return R(F7_NORMAL, shamt, rs1, F3_SRL_SRA, rd, OP_OP_IMM);
}
inline Word SRAI(uint8_t rd, uint8_t rs1, uint8_t shamt) {
    return R(F7_ALT, shamt, rs1, F3_SRL_SRA, rd, OP_OP_IMM);
}
inline Word BEQ(uint8_t rs1, uint8_t rs2, int16_t offset) {
    return B(offset, rs2, rs1, F3_BEQ, OP_BRANCH);
}
inline Word BNE(uint8_t rs1, uint8_t rs2, int16_t offset) {
    return B(offset, rs2, rs1, F3_BNE, OP_BRANCH);
}
inline Word MUL(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_MEXT, rs2, rs1, F3_MUL, rd, OP_OP);
}
inline Word DIV(uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return R(F7_MEXT, rs2, rs1, F3_DIV, rd, OP_OP);
}
inline Word ECALL() {
    return Word(OP_SYSTEM);
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

// CSR-type: [31:20]=csrAddr, [19:15]=rs1/zimm, [14:12]=funct3, [11:7]=rd
inline Word CSR(uint16_t csrAddr, uint8_t rs1, uint8_t funct3, uint8_t rd) {
    assert(csrAddr < 4096 && rd < 32 && rs1 < 32 && funct3 < 8 && "CSR: field out of range");
    return (Word(csrAddr) << 20) | (Word(rs1) << 15) | (Word(funct3) << 12) | (Word(rd) << 7) |
           Word(OP_SYSTEM);
}
inline Word CSRRW(uint8_t rd, uint16_t csr, uint8_t rs1) {
    return CSR(csr, rs1, F3_CSRRW, rd);
}
inline Word CSRRS(uint8_t rd, uint16_t csr, uint8_t rs1) {
    return CSR(csr, rs1, F3_CSRRS, rd);
}
inline Word CSRRC(uint8_t rd, uint16_t csr, uint8_t rs1) {
    return CSR(csr, rs1, F3_CSRRC, rd);
}
inline Word CSRRWI(uint8_t rd, uint16_t csr, uint8_t zimm) {
    return CSR(csr, zimm, F3_CSRRWI, rd);
}
inline Word CSRRSI(uint8_t rd, uint16_t csr, uint8_t zimm) {
    return CSR(csr, zimm, F3_CSRRSI, rd);
}
inline Word CSRRCI(uint8_t rd, uint16_t csr, uint8_t zimm) {
    return CSR(csr, zimm, F3_CSRRCI, rd);
}

// A-extension: AMO instructions
inline Word AMO(uint8_t funct5, bool aq, bool rl, uint8_t rs2, uint8_t rs1, uint8_t rd) {
    auto f7 = static_cast<uint8_t>((static_cast<uint32_t>(funct5) << 2u) | (aq ? 2u : 0u) |
                                   (rl ? 1u : 0u));
    return R(f7, rs2, rs1, F3_AMO_W, rd, OP_AMO);
}
inline Word LR_W(uint8_t rd, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_LR, aq, rl, 0, rs1, rd);
}
inline Word SC_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_SC, aq, rl, rs2, rs1, rd);
}
inline Word AMOSWAP_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOSWAP, aq, rl, rs2, rs1, rd);
}
inline Word AMOADD_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOADD, aq, rl, rs2, rs1, rd);
}
inline Word AMOXOR_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOXOR, aq, rl, rs2, rs1, rd);
}
inline Word AMOAND_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOAND, aq, rl, rs2, rs1, rd);
}
inline Word AMOOR_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOOR, aq, rl, rs2, rs1, rd);
}
inline Word AMOMIN_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOMIN, aq, rl, rs2, rs1, rd);
}
inline Word AMOMAX_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOMAX, aq, rl, rs2, rs1, rd);
}
inline Word AMOMINU_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOMINU, aq, rl, rs2, rs1, rd);
}
inline Word AMOMAXU_W(uint8_t rd, uint8_t rs2, uint8_t rs1, bool aq = false, bool rl = false) {
    return AMO(F5_AMOMAXU, aq, rl, rs2, rs1, rd);
}
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
    mem.loadProgram(std::move(bytes), base);
}

}  // namespace InstrBuilder