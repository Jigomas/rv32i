#include <cassert>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "../include/config.hpp"
#include "../include/dumper.hpp"
#include "../include/instr_builder.hpp"
#include "../include/memory_model.hpp"
#include "../include/rv_model.hpp"
#include "../include/types.hpp"
#include "cache_model.hpp"

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

static void run_os(const char* bin_path, bool debug, const std::string& trace_path) {
    std::cout << "=== XorOS ===\n";

    std::ifstream f(bin_path, std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error(std::string("cannot open: ") + bin_path);

    const auto size = f.tellg();
    f.seekg(0);

    constexpr std::size_t MEM_SIZE = 0x10000;  // 64 KiB
    MemoryModel<32>       mem(MEM_SIZE);
    f.read(reinterpret_cast<char*>(mem.data()), size);

    // UART: TX register at 0xF000 — write byte → stdout
    mem.mapMmio(
        0xF000u,
        4u,
        [](uint32_t) { return 0u; },
        [](uint32_t, uint32_t val) { std::cout << static_cast<char>(val & 0xFFu); });

    // CLINT: mtime at 0xF004 (lo/hi), mtimecmp at 0xF00C (lo/hi)
    // placed just above UART (0xF000) in the identity-mapped region
    uint64_t mtime    = 0;
    uint64_t mtimecmp = UINT64_MAX;  // disabled until OS writes it

    mem.mapMmio(
        0xF004u,
        8u,
        [&](uint32_t addr) -> uint32_t {
            return addr == 0xF004u ? static_cast<uint32_t>(mtime)
                                   : static_cast<uint32_t>(mtime >> 32);
        },
        [](uint32_t, uint32_t) {});  // mtime is read-only from software

    mem.mapMmio(
        0xF00Cu,
        8u,
        [&](uint32_t addr) -> uint32_t {
            return addr == 0xF00Cu ? static_cast<uint32_t>(mtimecmp)
                                   : static_cast<uint32_t>(mtimecmp >> 32);
        },
        [&](uint32_t addr, uint32_t val) {
            if (addr == 0xF00Cu)
                mtimecmp = (mtimecmp & 0xFFFFFFFF00000000ull) | val;
            else
                mtimecmp = (static_cast<uint64_t>(val) << 32) | (mtimecmp & 0xFFFFFFFFull);
        });

    CacheModel<32>              cache(mem, 64);
    Config                      cfg(Config::EXT_A);
    RVModel<32, CacheModel<32>> cpu(cfg, cache);
    cpu.init(0x0u, static_cast<uint32_t>(MEM_SIZE) - 4u);

    Dumper<32> dbg(trace_path);
    cpu.setDebug(debug);
    if (!trace_path.empty())
        cpu.setStepHook([&](uint32_t pc, const std::string& dis) { dbg.onStep(pc, dis); });
    if (debug || !trace_path.empty())
        cpu.setTrapHook([&](uint32_t cause, uint32_t mepc, uint32_t tval) {
            dbg.onTrap(cause, mepc, tval, mtime);
            dbg.onTrapEtl(cause, mepc, tval, mtime);
        });

    while (!cpu.isHalted()) {
        cpu.step();
        ++mtime;
        if (mtime >= mtimecmp)
            cpu.triggerInterrupt(CSR::INT_TIMER_M);
    }

    if (debug) {
        dbg.dumpState(cpu.regs(), cpu.csr(), cpu.getPC());
        dbg.dumpClint(mtime, mtimecmp);
        dbg.dumpMemHex(mem, 0x0000u, 0x200u);   // boot / text
        dbg.dumpMemHex(mem, 0x3000u, 0x1000u);  // BSS: procs, page tables
    }

    std::cout << "\ncache: " << cache.hits() << " hits / " << cache.misses() << " misses" << " | "
              << std::fixed << std::setprecision(1) << cache.hitRate() * 100.0 << "% hit rate\n";
}

// usage: rv32i_cpu <bin> [--debug] [--trace <file.txt>]
int main(int argc, char* argv[]) {
    if (argc >= 2 && argv[1][0] != '-') {
        bool        debug = false;
        std::string trace_path;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "--debug") == 0) {
                debug = true;
            } else if (std::strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
                trace_path = argv[++i];
            }
        }
        try {
            run_os(argv[1], debug, trace_path);
        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

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
