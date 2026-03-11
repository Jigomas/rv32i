#pragma once
#include "types.hpp"

class ALU {
public:
    ALU()                      = delete;
    ~ALU()                     = delete;
    ALU(const ALU&)            = delete;
    ALU& operator=(const ALU&) = delete;
    ALU(ALU&&)                 = delete;
    ALU& operator=(ALU&&)      = delete;

    enum class Op {
        // RV32I
        ADD,
        SUB,
        AND,
        OR,
        XOR,
        SLL,
        SRL,
        SRA,
        SLT,
        SLTU,
        // M extension
        MUL,
        MULH,
        MULHSU,
        MULHU,
        DIV,
        DIVU,
        REM,
        REMU,
    };

    static Word execute(Op op, Word a, Word b);
};