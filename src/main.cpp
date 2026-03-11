#include <cassert>
#include <iostream>

#include "../include/config.hpp"
#include "../include/cpu.hpp"
#include "../include/instr_builder.hpp"
#include "../include/memory.hpp"
#include "../include/types.hpp"


namespace Reg {
    constexpr uint8_t zero = 0;
    constexpr uint8_t ra   = 1;
    constexpr uint8_t sp   = 2;
    constexpr uint8_t a0   = 10;
    constexpr uint8_t a1   = 11;
    constexpr uint8_t a2   = 12;
    constexpr uint8_t a3   = 13;
    constexpr uint8_t a4   = 14;
}

int main() {
    std::cout << "===========================================\n";
    std::cout << "      RV32I Processor Demo  (XLEN=32)      \n";
    std::cout << "===========================================\n\n";

    try {
        Memory mem(4096);
        Config cfg(Config::EXT_M);

        // to calc: (3 + 4) * 5 = 35

        using namespace InstrBuilder;

        loadProgram(mem, {
            ADDI(Reg::a0, Reg::zero, 3),   // a0 = 3
            ADDI(Reg::a1, Reg::zero, 4),   // a1 = 4
            ADDI(Reg::a2, Reg::zero, 5),   // a2 = 5
            ADD (Reg::a3, Reg::a0,   Reg::a1),  // a3 = a0 + a1 = 7
            MUL (Reg::a4, Reg::a3,   Reg::a2),  // a4 = a3 * a2 = 35
            HALT()
        });

        CPU cpu(cfg, mem);
        cpu.setDebug(true);
        cpu.run();

        cpu.regs().dump();

        const Word result = cpu.regs().read(Reg::a4);
        std::cout << "\n(3 + 4) * 5 = " << result
                  << (result == 35u ? "  OK\n" : "  FAIL\n");
        assert(result == 35u);

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}