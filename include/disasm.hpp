#pragma once
#include <sstream>
#include <string>

#include "decoder.hpp"
#include "isa.hpp"

namespace Disasm {

inline const char* regName(uint8_t r) {
    static const char* names[32] = {
        "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
        "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
        "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
    };
    return (r < 32) ? names[r] : "?";
}

inline std::string csrName(uint16_t addr) {
    switch (addr) {
        case 0x180:
            return "satp";
        case 0x300:
            return "mstatus";
        case 0x301:
            return "misa";
        case 0x304:
            return "mie";
        case 0x305:
            return "mtvec";
        case 0x340:
            return "mscratch";
        case 0x341:
            return "mepc";
        case 0x342:
            return "mcause";
        case 0x343:
            return "mtval";
        case 0x344:
            return "mip";
        case 0xB00:
            return "mcycle";
        case 0xB02:
            return "minstret";
        case 0xF11:
            return "mvendorid";
        case 0xF12:
            return "marchid";
        case 0xF13:
            return "mimpid";
        case 0xF14:
            return "mhartid";
        default: {
            std::ostringstream ss;
            ss << "0x" << std::hex << addr;
            return ss.str();
        }
    }
}

template <int XLEN = 32>
inline std::string disassemble(const DecodedInstr<XLEN>& d) {
    using namespace ISA;

    const char* rd  = regName(d.rd);
    const char* rs1 = regName(d.rs1);
    const char* rs2 = regName(d.rs2);
    auto        imm = std::to_string(static_cast<int64_t>(d.imm));
    auto        mem = [&](const char* mn) {
        return std::string(mn) + " " + rd + ", " + imm + "(" + rs1 + ")";
    };
    auto rtype = [&](const char* mn) {
        return std::string(mn) + " " + rd + ", " + rs1 + ", " + rs2;
    };
    auto itype = [&](const char* mn) {
        return std::string(mn) + " " + rd + ", " + rs1 + ", " + imm;
    };
    auto btype = [&](const char* mn) {
        return std::string(mn) + " " + rs1 + ", " + rs2 + ", " + imm;
    };
    auto stype = [&](const char* mn) {
        return std::string(mn) + " " + rs2 + ", " + imm + "(" + rs1 + ")";
    };

    switch (d.opcode) {
        case OP_OP_IMM:
            switch (d.funct3) {
                case F3_ADD_SUB:
                    return itype("addi");
                case F3_SLT:
                    return itype("slti");
                case F3_SLTU:
                    return itype("sltiu");
                case F3_XOR:
                    return itype("xori");
                case F3_OR:
                    return itype("ori");
                case F3_AND:
                    return itype("andi");
                case F3_SLL:
                    return std::string("slli ") + rd + ", " + rs1 + ", " + std::to_string(d.rs2);
                case F3_SRL_SRA:
                    if (d.funct7 == F7_ALT)
                        return std::string("srai ") + rd + ", " + rs1 + ", " +
                               std::to_string(d.rs2);
                    return std::string("srli ") + rd + ", " + rs1 + ", " + std::to_string(d.rs2);
            }
            break;

        case OP_OP:
            if (d.funct7 == F7_MEXT) {
                switch (d.funct3) {
                    case F3_MUL:
                        return rtype("mul");
                    case F3_MULH:
                        return rtype("mulh");
                    case F3_MULHSU:
                        return rtype("mulhsu");
                    case F3_MULHU:
                        return rtype("mulhu");
                    case F3_DIV:
                        return rtype("div");
                    case F3_DIVU:
                        return rtype("divu");
                    case F3_REM:
                        return rtype("rem");
                    case F3_REMU:
                        return rtype("remu");
                }
            }
            switch (d.funct3) {
                case F3_ADD_SUB:
                    return rtype(d.funct7 == F7_ALT ? "sub" : "add");
                case F3_SLL:
                    return rtype("sll");
                case F3_SLT:
                    return rtype("slt");
                case F3_SLTU:
                    return rtype("sltu");
                case F3_XOR:
                    return rtype("xor");
                case F3_SRL_SRA:
                    return rtype(d.funct7 == F7_ALT ? "sra" : "srl");
                case F3_OR:
                    return rtype("or");
                case F3_AND:
                    return rtype("and");
            }
            break;

        case OP_LOAD:
            switch (d.funct3) {
                case F3_LB:
                    return mem("lb");
                case F3_LH:
                    return mem("lh");
                case F3_LW:
                    return mem("lw");
                case F3_LBU:
                    return mem("lbu");
                case F3_LHU:
                    return mem("lhu");
            }
            break;

        case OP_STORE:
            switch (d.funct3) {
                case F3_SB:
                    return stype("sb");
                case F3_SH:
                    return stype("sh");
                case F3_SW:
                    return stype("sw");
            }
            break;

        case OP_BRANCH:
            switch (d.funct3) {
                case F3_BEQ:
                    return btype("beq");
                case F3_BNE:
                    return btype("bne");
                case F3_BLT:
                    return btype("blt");
                case F3_BGE:
                    return btype("bge");
                case F3_BLTU:
                    return btype("bltu");
                case F3_BGEU:
                    return btype("bgeu");
            }
            break;

        case OP_LUI:
            return std::string("lui ") + rd + ", " + std::to_string(d.imm >> 12);

        case OP_AUIPC:
            return std::string("auipc ") + rd + ", " + std::to_string(d.imm >> 12);

        case OP_JAL:
            return std::string("jal ") + rd + ", " + imm;

        case OP_JALR:
            return std::string("jalr ") + rd + ", " + rs1 + ", " + imm;

        case OP_SYSTEM: {
            auto csr = csrName(static_cast<uint16_t>(static_cast<uint32_t>(d.imm) & 0xFFFu));
            if (d.funct3 == 0) {
                if (d.imm == 0)
                    return "ecall";
                if (d.imm == 1)
                    return "ebreak";
                if (d.imm == 0x302)
                    return "mret";
            }
            auto zimm = std::to_string(d.rs1);
            switch (d.funct3) {
                case F3_CSRRW:
                    return std::string("csrrw ") + rd + ", " + csr + ", " + rs1;
                case F3_CSRRS:
                    return std::string("csrrs ") + rd + ", " + csr + ", " + rs1;
                case F3_CSRRC:
                    return std::string("csrrc ") + rd + ", " + csr + ", " + rs1;
                case F3_CSRRWI:
                    return std::string("csrrwi ") + rd + ", " + csr + ", " + zimm;
                case F3_CSRRSI:
                    return std::string("csrrsi ") + rd + ", " + csr + ", " + zimm;
                case F3_CSRRCI:
                    return std::string("csrrci ") + rd + ", " + csr + ", " + zimm;
                default:
                    break;
            }
            break;
        }

        case OP_MISC_MEM:
            return "fence";

        case OP_AMO: {
            uint8_t funct5 = d.funct7 >> 2u;
            switch (funct5) {
                case F5_LR:
                    return std::string("lr.w ") + rd + ", (" + rs1 + ")";
                case F5_SC:
                    return std::string("sc.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOSWAP:
                    return std::string("amoswap.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOADD:
                    return std::string("amoadd.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOXOR:
                    return std::string("amoxor.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOOR:
                    return std::string("amoor.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOAND:
                    return std::string("amoand.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOMIN:
                    return std::string("amomin.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOMAX:
                    return std::string("amomax.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOMINU:
                    return std::string("amominu.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                case F5_AMOMAXU:
                    return std::string("amomaxu.w ") + rd + ", " + rs2 + ", (" + rs1 + ")";
                default:
                    break;
            }
            break;
        }
    }
    return "unknown " + d.toString();
}

}  // namespace Disasm
