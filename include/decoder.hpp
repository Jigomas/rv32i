#pragma once
#include <cassert>
#include <iomanip>
#include <sstream>
#include <string>

#include "isa.hpp"
#include "types.hpp"

// Decoded instruction — imm is sign-extended to the machine word width (XLEN bits)
template <int XLEN = 32>
struct DecodedInstr {
    using SWord = typename XlenTraits<XLEN>::SWord;

    uint8_t opcode;  // [6:0]
    uint8_t rd;      // [11:7]
    uint8_t rs1;     // [19:15]
    uint8_t rs2;     // [24:20]
    uint8_t funct3;  // [14:12]
    uint8_t funct7;  // [31:25]
    SWord   imm;     // sign-extended immediate (width = XLEN)
    Word    raw;     // raw 32-bit instruction word

    std::string toString() const {
        std::ostringstream o;
        o << std::hex << "RAW=0x" << std::setw(8) << std::setfill('0') << raw << " OP=0x"
          << static_cast<int>(opcode) << " rd=x" << std::dec << static_cast<int>(rd) << " rs1=x"
          << static_cast<int>(rs1) << " rs2=x" << static_cast<int>(rs2) << " f3=0x" << std::hex
          << static_cast<int>(funct3) << " f7=0x" << static_cast<int>(funct7) << " imm=" << std::dec
          << static_cast<int64_t>(imm);
        return o.str();
    }
};

template <int XLEN = 32>
class Decoder {
public:
    Decoder()                          = delete;
    ~Decoder()                         = delete;
    Decoder(const Decoder&)            = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&)                 = delete;
    Decoder& operator=(Decoder&&)      = delete;

    static DecodedInstr<XLEN> decode(Word raw);
};

template <int XLEN>
DecodedInstr<XLEN> Decoder<XLEN>::decode(Word raw) {
    using namespace ISA;
    assert(raw != 0u && "Decoder::decode — raw=0 is the HALT sentinel, not a valid instruction");

    DecodedInstr<XLEN> d;
    d.raw    = raw;
    d.opcode = getOpcode(raw);
    d.rd     = getRd(raw);
    d.funct3 = getFunct3(raw);
    d.rs1    = getRs1(raw);
    d.rs2    = getRs2(raw);
    d.funct7 = getFunct7(raw);

    // Invariant: register indices are 5-bit fields [0,31]
    assert(d.rd < 32 && "Decoder: rd out of range");
    assert(d.rs1 < 32 && "Decoder: rs1 out of range");
    assert(d.rs2 < 32 && "Decoder: rs2 out of range");
    assert(d.funct3 < 8 && "Decoder: funct3 out of range");
    assert(d.funct7 < 128 && "Decoder: funct7 out of range");

    switch (d.opcode) {
        case OP_LOAD:
        case OP_OP_IMM:
        case OP_JALR:
        case OP_MISC_MEM:
        case OP_SYSTEM:
            d.imm = decodeImmI<XLEN>(raw);
            break;
        case OP_STORE:
            d.imm = decodeImmS<XLEN>(raw);
            break;
        case OP_BRANCH:
            d.imm = decodeImmB<XLEN>(raw);
            break;
        case OP_LUI:
        case OP_AUIPC:
            d.imm = static_cast<typename DecodedInstr<XLEN>::SWord>(decodeImmU(raw));
            break;
        case OP_JAL:
            d.imm = decodeImmJ<XLEN>(raw);
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
