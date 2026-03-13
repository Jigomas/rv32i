#pragma once
#include <cassert>
#include <climits>
#include <stdexcept>

#include "types.hpp"

template <int XLEN = 32>
class ALU {
public:
    ALU()                      = delete;
    ~ALU()                     = delete;
    ALU(const ALU&)            = delete;
    ALU& operator=(const ALU&) = delete;
    ALU(ALU&&)                 = delete;
    ALU& operator=(ALU&&)      = delete;

    using UWord = typename XlenTraits<XLEN>::UWord;
    using SWord = typename XlenTraits<XLEN>::SWord;

    enum class Op {
        // RV32I / RV64I
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

    static UWord execute(Op op, UWord a, UWord b);
};

template <int XLEN>
typename ALU<XLEN>::UWord ALU<XLEN>::execute(Op op, UWord a, UWord b) {
    using Traits = XlenTraits<XLEN>;
    SWord sa     = static_cast<SWord>(a);
    SWord sb     = static_cast<SWord>(b);

    // Shift amounts are masked to XLEN-1 bits per spec (5 for RV32, 6 for RV64)
    const UWord shamt = b & static_cast<UWord>(Traits::SHIFT_MASK);

    switch (op) {
        case Op::ADD:
            return a + b;
        case Op::SUB:
            return a - b;
        case Op::AND:
            return a & b;
        case Op::OR:
            return a | b;
        case Op::XOR:
            return a ^ b;

        case Op::SLL:
            return a << shamt;
        case Op::SRL:
            return a >> shamt;
        case Op::SRA:
            return static_cast<UWord>(sa >> static_cast<int>(shamt));

        case Op::SLT:
            return (sa < sb) ? UWord(1) : UWord(0);
        case Op::SLTU:
            return (a < b) ? UWord(1) : UWord(0);

        // M extension — MUL: lower XLEN bits of 2*XLEN product
        case Op::MUL: {
            if constexpr (XLEN == 32) {
                return static_cast<UWord>(static_cast<uint64_t>(a) * static_cast<uint64_t>(b));
            } else {
#if defined(__GNUC__) || defined(__clang__)
                __extension__ typedef unsigned __int128 U128;
                return static_cast<UWord>(U128(a) * U128(b));
#else
                static_assert(XLEN != 64, "MUL for RV64 requires __int128 (GCC/Clang)");
                return 0;
#endif
            }
        }

        // MULH: upper XLEN bits of signed*signed 2*XLEN product
        case Op::MULH: {
            if constexpr (XLEN == 32) {
                int64_t r = int64_t(sa) * int64_t(sb);
                return static_cast<UWord>(static_cast<uint64_t>(r) >> 32);
            } else {
#if defined(__GNUC__) || defined(__clang__)
                __extension__ typedef __int128          S128;
                __extension__ typedef unsigned __int128 U128;
                S128                                    r = S128(sa) * S128(sb);
                return static_cast<UWord>(U128(r) >> 64);
#else
                static_assert(XLEN != 64, "MULH for RV64 requires __int128 (GCC/Clang)");
                return 0;
#endif
            }
        }

        // MULHSU: upper XLEN bits of signed(rs1) * unsigned(rs2) product
        case Op::MULHSU: {
            if constexpr (XLEN == 32) {
                int64_t r = int64_t(sa) * int64_t(static_cast<uint64_t>(b));
                return static_cast<UWord>(static_cast<uint64_t>(r) >> 32);
            } else {
#if defined(__GNUC__) || defined(__clang__)
                __extension__ typedef __int128          S128;
                __extension__ typedef unsigned __int128 U128;
                S128                                    r = S128(sa) * S128(U128(b));
                return static_cast<UWord>(U128(r) >> 64);
#else
                static_assert(XLEN != 64, "MULHSU for RV64 requires __int128 (GCC/Clang)");
                return 0;
#endif
            }
        }

        // MULHU: upper XLEN bits of unsigned*unsigned product
        case Op::MULHU: {
            if constexpr (XLEN == 32) {
                uint64_t r = uint64_t(a) * uint64_t(b);
                return static_cast<UWord>(r >> 32);
            } else {
#if defined(__GNUC__) || defined(__clang__)
                __extension__ typedef unsigned __int128 U128;
                U128                                    r = U128(a) * U128(b);
                return static_cast<UWord>(r >> 64);
#else
                static_assert(XLEN != 64, "MULHU for RV64 requires __int128 (GCC/Clang)");
                return 0;
#endif
            }
        }

        case Op::DIV: {
            if (sb == SWord(0))
                return Traits::UWORD_MAX;  // div-by-zero → -1
            if (sa == Traits::SWORD_MIN && sb == SWord(-1))
                return static_cast<UWord>(Traits::SWORD_MIN);
            return static_cast<UWord>(sa / sb);
        }
        case Op::DIVU: {
            if (b == UWord(0))
                return Traits::UWORD_MAX;
            return a / b;
        }
        case Op::REM: {
            if (sb == SWord(0))
                return a;
            if (sa == Traits::SWORD_MIN && sb == SWord(-1))
                return UWord(0);
            return static_cast<UWord>(sa % sb);
        }
        case Op::REMU: {
            if (b == UWord(0))
                return a;
            return a % b;
        }

        default:
            throw std::invalid_argument("ALU::execute — unknown Op value");
    }
}
