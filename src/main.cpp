#include <cassert>
#include <iostream>

#include "../include/config.hpp"
#include "../include/instr_builder.hpp"
#include "../include/memory_model.hpp"
#include "../include/rv_model.hpp"
#include "../include/types.hpp"

// ABI register aliases for readability
namespace Reg {
constexpr uint8_t zero = 0;
constexpr uint8_t a0   = 10;
constexpr uint8_t a1   = 11;
constexpr uint8_t a2   = 12;
constexpr uint8_t a3   = 13;
constexpr uint8_t a4   = 14;
}  // namespace Reg

static void demo_arithmetic() {
    std::cout << "=== Demo: (3 + 4) * 5 ===\n";

    MemoryModel<32> mem(4096);
    Config          cfg(Config::EXT_M);

    using namespace InstrBuilder;
    loadProgram(mem,
                {ADDI(Reg::a0, Reg::zero, 3),     // a0 = 3
                 ADDI(Reg::a1, Reg::zero, 4),     // a1 = 4
                 ADDI(Reg::a2, Reg::zero, 5),     // a2 = 5
                 ADD(Reg::a3, Reg::a0, Reg::a1),  // a3 = 3 + 4 = 7
                 MUL(Reg::a4, Reg::a3, Reg::a2),  // a4 = 7 * 5 = 35
                 HALT()});

    RVModel<32> cpu(cfg, mem);
    cpu.setDebug(true);
    cpu.run();
    cpu.regs().dump();

    const Word result = cpu.regs().get(Reg::a4);
    std::cout << "\n(3 + 4) * 5 = " << result << (result == 35u ? "  OK\n" : "  FAIL\n");
    assert(result == 35u);
}

int main() {
    std::cout << "===========================================\n";
    std::cout << "      RV32I Processor Demo  (XLEN=32)      \n";
    std::cout << "===========================================\n\n";

    try {
        demo_arithmetic();
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

// TODO: CSR-регистры и механизм прерываний
// TODO: ELF-загрузчик
// TODO: дизассемблер (DecodedInstr → "ADDI a0, zero, 42")
// TODO: MMIO через callback-map в MemoryModel
// TODO: расширения A / F / D / C