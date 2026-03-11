#pragma once
#include <array>
#include <cstdint>

#include "types.hpp"

class RegisterFile {
public:
    static constexpr int NUM_REGS = 32;
    static const char*   ABI_NAMES[NUM_REGS];

    RegisterFile();
    ~RegisterFile()                                    = default;
    RegisterFile(const RegisterFile&)                  = default;
    RegisterFile& operator=(const RegisterFile&)       = default;
    RegisterFile(RegisterFile&&) noexcept              = default;
    RegisterFile& operator=(RegisterFile&&) noexcept   = default;

    Word get(std::size_t idx) const;
    void set(std::size_t idx, Word val);

    Word read(uint8_t idx) const { return get(idx); }
    void write(uint8_t idx, Word val) { set(idx, val); }

    void dump() const;

private:
    std::array<Word, NUM_REGS> regs_;
};