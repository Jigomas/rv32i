#include "../include/registers.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>

// ABI-names in order x0–x31
const char* Register::ABI_NAMES[NUM_REGS] = {"zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                                             "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                                             "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                                             "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

Register::Register() : pc_(0u) {
    regs_.fill(0u);
}

Word Register::read(uint8_t idx) const {
    assert(idx < NUM_REGS);
    return (idx == 0) ? 0u : regs_[idx];  // x0 = hardwired zero
}

void Register::write(uint8_t idx, Word val) {
    assert(idx < NUM_REGS);
    if (idx != 0)
        regs_[idx] = val;  // Writing in x0 — no-op
}

Word Register::getPC() const {
    return pc_;
}
void Register::setPC(Word val) {
    pc_ = val;
}
void Register::advancePC() {
    pc_ += 4u;
}

void Register::dump() const {
    std::cout << "\n=== REGISTER FILE ===\n";
    std::cout << "PC  = 0x" << std::hex << std::setw(8) << std::setfill('0') << pc_ << std::dec
              << "\n";
    for (int i = 0; i < NUM_REGS; ++i) {
        std::cout << "x" << std::setw(2) << std::setfill('0') << i << " (" << std::setw(4)
                  << std::setfill(' ') << ABI_NAMES[i] << "): " << "0x" << std::hex << std::setw(8)
                  << std::setfill('0') << regs_[i] << "  (" << std::dec
                  << static_cast<SWord>(regs_[i]) << ")\n";
    }
}