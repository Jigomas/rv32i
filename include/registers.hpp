#pragma once
#include <array>
#include <cstdint>

#include "types.hpp"

class Register {
public:
    static constexpr int NUM_REGS = 32;
    static const char*   ABI_NAMES[NUM_REGS];

    Register();
    ~Register() = default;

    Register(const Register& other)                = default;
    Register& operator=(const Register& other)     = default;
    Register(Register&& other) noexcept            = default;
    Register& operator=(Register&& other) noexcept = default;

    Word read(uint8_t idx) const;
    void write(uint8_t idx, Word val);

    Word get(std::size_t idx) const { return read(static_cast<uint8_t>(idx)); }
    void set(std::size_t idx, Word v) { write(static_cast<uint8_t>(idx), v); }

    Word getPC() const;
    void setPC(Word val);
    void advancePC();

    void dump() const;

private:
    std::array<Word, NUM_REGS> regs_;
    Word                       pc_;
};