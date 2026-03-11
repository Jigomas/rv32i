#pragma once
#include <string>

#include "types.hpp"

struct DecodedInstr {
    uint8_t opcode;  // [6:0]  — opcode field
    uint8_t rd;      // [11:7] — destination register
    uint8_t rs1;     // [19:15]— source register 1
    uint8_t rs2;     // [24:20]— source register 2
    uint8_t funct3;  // [14:12]— 3-bit function field
    uint8_t funct7;  // [31:25]— 7-bit function field
    SWord   imm;     // Sign-extended immediate (format depends on opcode)
    Word    raw;     // Raw instruction word

    std::string toString() const;  
};

class Decoder {
public:
    Decoder()                          = delete;
    ~Decoder()                         = delete;
    Decoder(const Decoder&)            = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&)                 = delete;
    Decoder& operator=(Decoder&&)      = delete;

    static DecodedInstr decode(Word raw);
};