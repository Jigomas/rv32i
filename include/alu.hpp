#pragma once
#include "types.hpp"

class ALU {
public:
    enum class Op {
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