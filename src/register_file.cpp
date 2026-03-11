#include "../include/register_file.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>

const char* RegisterFile::ABI_NAMES[NUM_REGS] = {
    "zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
    "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
    "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

RegisterFile::RegisterFile() {
    regs_.fill(0u);
}

Word RegisterFile::get(std::size_t idx) const {
    assert(idx < static_cast<size_t>(NUM_REGS) && "RegisterFile::get — index out of range");
    return (idx == 0) ? 0u : regs_[idx];  // x0 hardwired to zero
}

void RegisterFile::set(std::size_t idx, Word val) {
    assert(idx < static_cast<size_t>(NUM_REGS) && "RegisterFile::set — index out of range");
    if (idx != 0)
        regs_[idx] = val;  // write to x0 is no-op
}

void RegisterFile::dump() const {
    std::cout << "\n=== REGISTER FILE ===\n";
    for (size_t i = 0; i < NUM_REGS; ++i) {
        std::cout << "x" << std::setw(2) << std::setfill('0') << i << " (" << std::setw(4)
                  << std::setfill(' ') << ABI_NAMES[i] << "): 0x" << std::hex << std::setw(8)
                  << std::setfill('0') << regs_[i] << "  (" << std::dec
                  << static_cast<SWord>(regs_[i]) << ")\n";
    }
}