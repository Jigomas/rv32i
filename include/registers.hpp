#pragma once
#include <array>
#include <cstdint>

#include "types.hpp"

class Register {
public:
    static constexpr int NUM_REGS = 32;

    static const char* ABI_NAMES[NUM_REGS];

    Register();

    Word read(uint8_t idx) const;
    void write(uint8_t idx, Word val);

    Word getPC() const;
    void setPC(Word val);
    void advancePC();

    void dump() const;

private:
    std::array<Word, NUM_REGS> regs_;
    Word                       pc_;
};