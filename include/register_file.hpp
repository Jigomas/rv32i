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

    // RISC-V calling convention: preserved = callee-saved, must survive a call
    enum class RegRole : uint8_t { Preserved, NonPreserved, Special };
    static const RegRole REG_ROLES[NUM_REGS];

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

    static RegRole role(std::size_t idx) {
        assert(idx < static_cast<std::size_t>(NUM_REGS));
        return REG_ROLES[idx];
    }
    static bool isPreserved(std::size_t idx) { return role(idx) == RegRole::Preserved; }
    static bool isNonPreserved(std::size_t idx) { return role(idx) == RegRole::NonPreserved; }
    static bool isSpecial(std::size_t idx) { return role(idx) == RegRole::Special; }

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

template <int XLEN>
const char* RegisterFile<XLEN>::ABI_NAMES[] = {"zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                                               "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                                               "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                                               "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

// x0=zero x1=ra x2=sp x3=gp x4=tp — Special
// x5–x7=t0–t2, x28–x31=t3–t6    — NonPreserved (caller-saved temporaries)
// x8–x9=s0–s1, x18–x27=s2–s11   — Preserved    (callee-saved)
// x10–x17=a0–a7                  — NonPreserved (caller-saved arguments/return values)
template <int XLEN>
const typename RegisterFile<XLEN>::RegRole RegisterFile<XLEN>::REG_ROLES[] = {
    RegRole::Special,       // x0  zero
    RegRole::Special,       // x1  ra
    RegRole::Special,       // x2  sp
    RegRole::Special,       // x3  gp
    RegRole::Special,       // x4  tp
    RegRole::NonPreserved,  // x5  t0
    RegRole::NonPreserved,  // x6  t1
    RegRole::NonPreserved,  // x7  t2
    RegRole::Preserved,     // x8  s0/fp
    RegRole::Preserved,     // x9  s1
    RegRole::NonPreserved,  // x10 a0
    RegRole::NonPreserved,  // x11 a1
    RegRole::NonPreserved,  // x12 a2
    RegRole::NonPreserved,  // x13 a3
    RegRole::NonPreserved,  // x14 a4
    RegRole::NonPreserved,  // x15 a5
    RegRole::NonPreserved,  // x16 a6
    RegRole::NonPreserved,  // x17 a7
    RegRole::Preserved,     // x18 s2
    RegRole::Preserved,     // x19 s3
    RegRole::Preserved,     // x20 s4
    RegRole::Preserved,     // x21 s5
    RegRole::Preserved,     // x22 s6
    RegRole::Preserved,     // x23 s7
    RegRole::Preserved,     // x24 s8
    RegRole::Preserved,     // x25 s9
    RegRole::Preserved,     // x26 s10
    RegRole::Preserved,     // x27 s11
    RegRole::NonPreserved,  // x28 t3
    RegRole::NonPreserved,  // x29 t4
    RegRole::NonPreserved,  // x30 t5
    RegRole::NonPreserved,  // x31 t6
};
