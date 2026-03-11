#include "../include/alu.hpp"

#include <cassert>
#include <climits>
#include <stdexcept>

Word ALU::execute(Op op, Word a, Word b) {
    SWord sa = static_cast<SWord>(a);
    SWord sb = static_cast<SWord>(b);

    switch (op) {
        // RV32I: arithmetic
        case Op::ADD:
            return a + b;
        case Op::SUB:
            return a - b;

        // RV32I: logic
        case Op::AND:
            return a & b;
        case Op::OR:
            return a | b;
        case Op::XOR:
            return a ^ b;

        // RV32I: shifts — mask to 5 bits (max shift = 31 for 32-bit word)
        case Op::SLL:
            return a << (b & 0x1Fu);
        case Op::SRL:
            return a >> (b & 0x1Fu);
        case Op::SRA:
            return static_cast<Word>(sa >> (b & 0x1Fu));

        case Op::SLT:
            return (sa < sb) ? 1u : 0u;
        case Op::SLTU:
            return (a < b) ? 1u : 0u;

        case Op::MUL: {
            return static_cast<Word>(static_cast<uint64_t>(a) * b);
        }
        case Op::MULH: {
            int64_t r = static_cast<int64_t>(sa) * static_cast<int64_t>(sb);
            return static_cast<Word>(static_cast<uint64_t>(r) >> 32);
        }
        case Op::MULHSU: {
            int64_t r = static_cast<int64_t>(sa) * static_cast<int64_t>(b);
            return static_cast<Word>(static_cast<uint64_t>(r) >> 32);
        }
        case Op::MULHU: {
            uint64_t r = static_cast<uint64_t>(a) * b;
            return static_cast<Word>(r >> 32);
        }

        case Op::DIV: {
            // RISC-V spec: div-by-zero = -1
            if (sb == 0)
                return static_cast<Word>(-1);
            // RISC-V spec: INT_MIN / -1 = INT_MIN (no exception)
            if (sa == INT32_MIN && sb == -1)
                return static_cast<Word>(INT32_MIN);
            return static_cast<Word>(sa / sb);
        }
        case Op::DIVU: {
            if (b == 0)
                return 0xFFFFFFFFu;
            return a / b;
        }
        case Op::REM: {
            if (sb == 0)
                return a;
            if (sa == INT32_MIN && sb == -1)
                return 0u;
            return static_cast<Word>(sa % sb);
        }
        case Op::REMU: {
            if (b == 0)
                return a;
            return a % b;
        }

        default:
            throw std::invalid_argument("ALU::execute — unknown ALU::Op value");
    }
}