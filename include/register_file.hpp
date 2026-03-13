#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "types.hpp"

template <int XLEN = 32>
class RegisterFile {
public:
    using UWord = typename XlenTraits<XLEN>::UWord;
    using SWord = typename XlenTraits<XLEN>::SWord;

    static constexpr int NUM_REGS = 32;
    static const char*   ABI_NAMES[NUM_REGS];

    RegisterFile() { regs_.fill(UWord(0)); }
    ~RegisterFile()                                  = default;
    RegisterFile(const RegisterFile&)                = default;
    RegisterFile& operator=(const RegisterFile&)     = default;
    RegisterFile(RegisterFile&&) noexcept            = default;
    RegisterFile& operator=(RegisterFile&&) noexcept = default;

    UWord get(std::size_t idx) const {
        assert(idx < static_cast<std::size_t>(NUM_REGS) &&
               "RegisterFile::get — index out of range");
        return (idx == 0) ? UWord(0) : regs_[idx];  // x0 hardwired to zero
    }

    void set(std::size_t idx, UWord val) {
        assert(idx < static_cast<std::size_t>(NUM_REGS) &&
               "RegisterFile::set — index out of range");
        if (idx != 0)
            regs_[idx] = val;  // write to x0 is no-op
    }

    UWord read(uint8_t idx) const { return get(idx); }
    void  write(uint8_t idx, UWord val) { set(idx, val); }

    void dump() const {
        std::cout << "\n=== REGISTER FILE (XLEN=" << XLEN << ") ===\n";
        for (std::size_t i = 0; i < static_cast<std::size_t>(NUM_REGS); ++i) {
            std::cout << "x" << std::setw(2) << std::setfill('0') << i << " (" << std::setw(4)
                      << std::setfill(' ') << ABI_NAMES[i] << "): 0x" << std::hex
                      << std::setw(XLEN / 4) << std::setfill('0') << regs_[i] << "  (" << std::dec
                      << static_cast<SWord>(regs_[i]) << ")\n";
        }
    }

private:
    std::array<UWord, NUM_REGS> regs_;
};

// Static member definition — valid in header for class templates (template ODR)
template <int XLEN>
const char* RegisterFile<XLEN>::ABI_NAMES[] = {"zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                                               "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                                               "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                                               "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
