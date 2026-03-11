#include "../include/decoder.hpp"

#include <cassert>
#include <iomanip>
#include <sstream>

#include "../include/isa.hpp"

std::string DecodedInstr::toString() const {
    std::ostringstream o;
    o << std::hex << "RAW=0x" << std::setw(8) << std::setfill('0') << raw << " OP=0x"
      << static_cast<int>(opcode) << " rd=x" << std::dec << static_cast<int>(rd) << " rs1=x"
      << static_cast<int>(rs1) << " rs2=x" << static_cast<int>(rs2) << " f3=0x" << std::hex
      << static_cast<int>(funct3) << " f7=0x" << static_cast<int>(funct7) << " imm=" << std::dec
      << imm;
    return o.str();
}

DecodedInstr Decoder::decode(Word raw) {
    using namespace ISA;

    assert(raw != 0u && "Decoder::decode — raw=0 is the HALT sentinel, not a valid instruction");

    DecodedInstr d;
    d.raw    = raw;
    d.opcode = getOpcode(raw);
    d.rd     = getRd(raw);
    d.funct3 = getFunct3(raw);
    d.rs1    = getRs1(raw);
    d.rs2    = getRs2(raw);
    d.funct7 = getFunct7(raw);

    switch (d.opcode) {
        case OP_LOAD:
        case OP_OP_IMM:
        case OP_JALR:
        case OP_MISC_MEM:
        case OP_SYSTEM:
            d.imm = decodeImmI(raw);
            break;
        case OP_STORE:
            d.imm = decodeImmS(raw);
            break;
        case OP_BRANCH:
            d.imm = decodeImmB(raw);
            break;
        case OP_LUI:
        case OP_AUIPC:
            d.imm = static_cast<SWord>(decodeImmU(raw));
            break;
        case OP_JAL:
            d.imm = decodeImmJ(raw);
            break;
        case OP_OP:
            d.imm = 0;
            break;

        default:
            d.imm = 0;
            break;
    }

    return d;
}