#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../include/alu.hpp"
#include "../include/config.hpp"
#include "../include/decoder.hpp"
#include "../include/disasm.hpp"
#include "../include/instr_builder.hpp"
#include "../include/isa.hpp"
#include "../include/memory_model.hpp"
#include "../include/register_file.hpp"
#include "../include/rv_model.hpp"
#include "../include/types.hpp"

static int passed = 0;
static int failed = 0;

static void check(const char* name, bool condition) {
    if (condition) {
        std::cout << "  PASS  " << name << "\n";
        ++passed;
    } else {
        std::cout << "  FAIL  " << name << "\n";
        ++failed;
    }
}

#define CHECK_THROWS(name, ExType, expr) \
    {                                    \
        bool caught_ = false;            \
        try {                            \
            expr;                        \
        } catch (const ExType&) {        \
            caught_ = true;              \
        }                                \
        check(name, caught_);            \
    }

struct Fixture {
    MemoryModel<32> mem;
    RVModel<32>     cpu;

    Fixture(const std::vector<Word>& prog, uint32_t ext = Config::EXT_NONE)
        : mem(4096), cpu(Config(ext), mem) {
        InstrBuilder::loadProgram(mem, prog);
    }

    Word run(uint8_t reg) {
        cpu.run();
        return cpu.regs().get(reg);
    }
};

static void test_memory() {
    std::cout << "\n[ MemoryModel ]\n";

    MemoryModel<32> m(256);

    m.write(Addr(0), ByteT(0xAB));
    check("write/readByte", m.readByte(0) == 0xABu);

    m.write(Addr(4), HalfT(0x1234));
    check("write/readHalf", m.readHalf(4) == 0x1234u);
    check("little-endian low byte", m.readByte(4) == 0x34u);
    check("little-endian high byte", m.readByte(5) == 0x12u);

    m.write(Addr(8), WordT(0xDEADBEEFu));
    check("write/readWord", m.readWord(8) == 0xDEADBEEFu);

    CHECK_THROWS("OOB read throws", std::out_of_range, m.readWord(254));
    CHECK_THROWS("OOB write throws", std::out_of_range, m.write(Addr(255), WordT(0)));
    CHECK_THROWS("loadProgram OOB", std::out_of_range, m.loadProgram(std::vector<uint8_t>(512, 0)));

    MemoryModel<32> copy = m;
    copy.write(Addr(0), ByteT(0));
    check("copy is deep", m.readByte(0) == 0xABu);
}

static void test_register_file() {
    std::cout << "\n[ RegisterFile ]\n";

    RegisterFile<32> r;

    r.set(1, 42u);
    check("set/get", r.get(1) == 42u);
    check("x0 always zero", r.get(0) == 0u);

    r.set(0, 0xFFFFFFFFu);  // write to x0 is no-op
    check("write x0 is no-op", r.get(0) == 0u);

    RegisterFile<32> copy = r;
    copy.set(1, 99u);
    check("copy is deep", r.get(1) == 42u);
}

static void test_alu() {
    std::cout << "\n[ ALU ]\n";

    check("ADD", ALU<32>::execute(ALU<32>::Op::ADD, 10u, 20u) == 30u);
    check("SUB", ALU<32>::execute(ALU<32>::Op::SUB, 30u, 7u) == 23u);
    check("AND", ALU<32>::execute(ALU<32>::Op::AND, 0xF0u, 0xFFu) == 0xF0u);
    check("OR", ALU<32>::execute(ALU<32>::Op::OR, 0xF0u, 0x0Fu) == 0xFFu);
    check("XOR", ALU<32>::execute(ALU<32>::Op::XOR, 0xFFu, 0x0Fu) == 0xF0u);
    check("SLL", ALU<32>::execute(ALU<32>::Op::SLL, 1u, 4u) == 16u);
    check("SRL", ALU<32>::execute(ALU<32>::Op::SRL, 16u, 2u) == 4u);

    // SRA preserves sign
    check("SRA",
          static_cast<SWord>(ALU<32>::execute(ALU<32>::Op::SRA, static_cast<Word>(-8), 2u)) == -2);

    // SLT/SLTU
    check("SLT signed", ALU<32>::execute(ALU<32>::Op::SLT, static_cast<Word>(-1), 1u) == 1u);
    check("SLTU unsigned", ALU<32>::execute(ALU<32>::Op::SLTU, 0xFFFFFFFFu, 1u) == 0u);

    // MUL
    check("MUL", ALU<32>::execute(ALU<32>::Op::MUL, 7u, 6u) == 42u);

    // division corner cases (RISC-V spec)
    check("DIV by zero", ALU<32>::execute(ALU<32>::Op::DIV, 5u, 0u) == static_cast<Word>(-1));
    check("DIVU by zero", ALU<32>::execute(ALU<32>::Op::DIVU, 5u, 0u) == 0xFFFFFFFFu);
    check("REM by zero", ALU<32>::execute(ALU<32>::Op::REM, 7u, 0u) == 7u);
    check("DIV INT_MIN/-1",
          ALU<32>::execute(ALU<32>::Op::DIV, static_cast<Word>(INT32_MIN), static_cast<Word>(-1)) ==
              static_cast<Word>(INT32_MIN));
}

static void test_decoder() {
    std::cout << "\n[ Decoder ]\n";

    // I-type: ADDI x10, x0, 42
    {
        DecodedInstr d = Decoder<32>::decode(InstrBuilder::ADDI(10, 0, 42));
        check("I-type opcode", d.opcode == ISA::OP_OP_IMM);
        check("I-type rd", d.rd == 10u);
        check("I-type imm", d.imm == 42);
    }
    // R-type: ADD x12, x10, x11
    {
        DecodedInstr d = Decoder<32>::decode(InstrBuilder::ADD(12, 10, 11));
        check("R-type opcode", d.opcode == ISA::OP_OP);
        check("R-type rd", d.rd == 12u);
        check("R-type rs1", d.rs1 == 10u);
        check("R-type rs2", d.rs2 == 11u);
    }
    // S-type: SW x10, 8(x11)
    {
        DecodedInstr d = Decoder<32>::decode(InstrBuilder::SW(10, 11, 8));
        check("S-type opcode", d.opcode == ISA::OP_STORE);
        check("S-type imm", d.imm == 8);
    }
    // B-type: negative offset
    {
        DecodedInstr d =
            Decoder<32>::decode(InstrBuilder::B(-8, 0, 0, ISA::F3_BEQ, ISA::OP_BRANCH));
        check("B-type negative imm", d.imm == -8);
    }
}

static void test_cpu() {
    std::cout << "\n[ RVModel ]\n";

    check("ADDI a0=42",
          Fixture({InstrBuilder::ADDI(10, 0, 42), InstrBuilder::HALT()}).run(10) == 42u);

    check("x0 always zero",
          Fixture({InstrBuilder::ADDI(0, 0, 99), InstrBuilder::HALT()}).run(0) == 0u);

    // ADD + SUB
    {
        Fixture f({InstrBuilder::ADDI(10, 0, 10),
                   InstrBuilder::ADDI(11, 0, 20),
                   InstrBuilder::ADD(12, 10, 11),
                   InstrBuilder::SUB(13, 12, 10),
                   InstrBuilder::HALT()});
        f.cpu.run();
        check("ADD result", f.cpu.regs().get(12) == 30u);
        check("SUB result", f.cpu.regs().get(13) == 20u);
    }

    // LUI
    {
        using namespace ISA;
        Fixture f(
            {InstrBuilder::U(static_cast<int32_t>(0x12345000u), 10, OP_LUI), InstrBuilder::HALT()});
        check("LUI", f.run(10) == 0x12345000u);
    }

    // SW / LW round-trip
    {
        Fixture f({InstrBuilder::ADDI(10, 0, 0x7F),
                   InstrBuilder::ADDI(11, 0, 512),
                   InstrBuilder::SW(10, 11, 0),
                   InstrBuilder::LW(12, 11, 0),
                   InstrBuilder::HALT()});
        check("SW/LW round-trip", f.run(12) == 0x7Fu);
    }

    // LB sign-extension
    {
        using namespace ISA;
        using namespace InstrBuilder;
        Fixture f({ADDI(10, 0, -1),
                   ADDI(11, 0, 512),
                   S(0, 10, 11, F3_SB, OP_STORE),  // mem[512] = 0xFF
                   I(0, 11, F3_LB, 12, OP_LOAD),   // a2 = sign_ext(0xFF) = -1
                   HALT()});
        check("LB sign-extend", static_cast<SWord>(f.run(12)) == SWord(-1));
    }

    // BEQ taken - skips one instruction
    {
        using namespace InstrBuilder;
        Fixture f({B(8, 0, 0, ISA::F3_BEQ, ISA::OP_BRANCH),
                   ADDI(10, 0, 99),  // skipped
                   ADDI(10, 0, 42),  // executed
                   HALT()});
        check("BEQ taken skips instr", f.run(10) == 42u);
    }

    // Loop: sum 1..10 = 55
    {
        using namespace InstrBuilder;
        Fixture f({ADDI(10, 0, 0),
                   ADDI(11, 0, 1),
                   ADDI(12, 0, 10),
                   ADD(10, 10, 11),
                   ADDI(11, 11, 1),
                   B(-8, 11, 12, ISA::F3_BGE, ISA::OP_BRANCH),
                   HALT()});
        check("loop sum 1..10 = 55", f.run(10) == 55u);
    }

    // JAL: skips instruction, sets ra
    {
        using namespace InstrBuilder;
        Fixture f({ADDI(10, 0, 1),
                   JAL(1, 8),          // jump to [12], ra = 8
                   ADDI(10, 10, 100),  // skipped
                   ADDI(10, 10, 1),    // a0 = 2
                   HALT()});
        f.cpu.run();
        check("JAL result", f.cpu.regs().get(10) == 2u);
        check("JAL ra saved", f.cpu.regs().get(1) == 8u);
    }

    // M extension
    {
        using namespace InstrBuilder;
        Fixture f({ADDI(10, 0, 7),
                   ADDI(11, 0, 6),
                   MUL(12, 10, 11),
                   R(ISA::F7_MEXT, 10, 12, ISA::F3_DIV, 13, ISA::OP_OP),
                   R(ISA::F7_MEXT, 11, 12, ISA::F3_REM, 14, ISA::OP_OP),
                   HALT()},
                  Config::EXT_M);
        f.cpu.run();
        check("MUL 7*6=42", f.cpu.regs().get(12) == 42u);
        check("DIV 42/7=6", f.cpu.regs().get(13) == 6u);
        check("REM 42%6=0", f.cpu.regs().get(14) == 0u);
    }

    {
        Fixture f({InstrBuilder::ADDI(10, 0, 7),
                   InstrBuilder::ADDI(11, 0, 6),
                   InstrBuilder::MUL(12, 10, 11),
                   InstrBuilder::HALT()},
                  Config::EXT_NONE);
        f.cpu.run();
        check("M ext disabled halts CPU", f.cpu.isHalted());
    }

    CHECK_THROWS(
        "illegal opcode throws", std::runtime_error, Fixture({Word(0xFFFFFFFFu)}).cpu.run());

    // init() resets state
    {
        using namespace InstrBuilder;
        MemoryModel<> mem(4096);
        loadProgram(mem, {ADDI(10, 0, 7), HALT()});
        RVModel cpu(Config{}, mem);
        cpu.run();
        check("first run result", cpu.regs().get(10) == 7u);
        cpu.init(0);
        cpu.run();
        check("init() resets and reruns", cpu.regs().get(10) == 7u);
    }

    // getPC() reflects PC
    {
        using namespace InstrBuilder;
        MemoryModel<> mem(4096);
        loadProgram(mem, {ADDI(10, 0, 1), HALT()});
        RVModel cpu(Config{}, mem);
        check("PC starts at 0", cpu.getPC() == 0u);
        cpu.step();
        check("PC advances to 4", cpu.getPC() == 4u);
    }
}

static void test_alignment() {
    std::cout << "\n[ Alignment checks ]\n";
    using namespace InstrBuilder;
    using namespace ISA;

    // LW at odd address - halts when mtvec=0
    {
        MemoryModel<32> mem(4096);
        RVModel<32>     cpu(Config{}, mem);
        loadProgram(mem, {ADDI(10, 0, 1), LW(11, 10, 0), HALT()});
        cpu.run();
        check("LW misalign halts CPU", cpu.isHalted());
    }

    // SW at address 2 - halts when mtvec=0
    {
        MemoryModel<32> mem(4096);
        RVModel<32>     cpu(Config{}, mem);
        loadProgram(mem, {ADDI(10, 0, 2), ADDI(11, 0, 1), SW(11, 10, 0), HALT()});
        cpu.run();
        check("SW misalign halts CPU", cpu.isHalted());
    }

    // LH at odd address - halts when mtvec=0
    {
        MemoryModel<32> mem(4096);
        RVModel<32>     cpu(Config{}, mem);
        loadProgram(mem, {ADDI(10, 0, 1), I(0, 10, F3_LH, 11, OP_LOAD), HALT()});
        cpu.run();
        check("LH misalign halts CPU", cpu.isHalted());
    }

    // SH at odd address - halts when mtvec=0
    {
        MemoryModel<32> mem(4096);
        RVModel<32>     cpu(Config{}, mem);
        loadProgram(mem,
                    {ADDI(10, 0, 1), ADDI(11, 0, 0x42), S(0, 11, 10, F3_SH, OP_STORE), HALT()});
        cpu.run();
        check("SH misalign halts CPU", cpu.isHalted());
    }

    // LW misalign with mtvec set - trap fires, mcause = EXC_LOAD_MISALIGN
    {
        MemoryModel<32> mem(4096);
        RVModel<32>     cpu(Config{}, mem);
        // trap handler at 0x40: HALT
        loadProgram(mem, {HALT()}, Addr(0x40));
        loadProgram(mem,
                    {ADDI(10, 0, 0x40),         // x10 = trap address
                     CSRRW(0, CSR::MTVEC, 10),  // mtvec = 0x40
                     ADDI(11, 0, 1),            // x11 = misaligned address
                     LW(12, 11, 0),             // LW from addr 1 -> misalign trap
                     HALT()});
        cpu.run();
        check("LW misalign mcause", cpu.csr().getMCAUSE() == CSR::EXC_LOAD_MISALIGN);
        check("LW misalign mepc", cpu.csr().getMEPC() == 12u);
        check("LW misalign mtval", cpu.csr().getMTVAL() == 1u);
    }
}

static void test_vmem() {
    std::cout << "\n[ Sv32 vmem ]\n";
    using namespace InstrBuilder;
    using namespace ISA;

    // bare mode (satp=0): load/store works without page tables
    {
        MemoryModel<32> mem(4096);
        RVModel<32>     cpu(Config{}, mem);
        loadProgram(mem,
                    {ADDI(10, 0, 512),   // x10 = data address
                     ADDI(11, 0, 0x7F),  // x11 = test value
                     SW(11, 10, 0),      // mem[512] = 0x7F
                     LW(12, 10, 0),      // x12 = mem[512]
                     HALT()});
        cpu.run();
        check("bare mode SW/LW", cpu.regs().get(12) == 0x7Fu);
    }

    // Sv32 identity map: virtual==physical, load/store works after enabling paging
    //   memory layout (16 KiB):
    //     0x0000 - program code
    //     0x1000 - data page
    //     0x2000 - root page table (entry 0 -> leaf)
    //     0x3000 - leaf page table (VPN[0]=0..3 identity mapped)
    {
        MemoryModel<32> mem(0x4000);
        RVModel<32>     cpu(Config{}, mem);

        // root_pt[0]: non-leaf pointing to leaf at 0x3000
        //   leaf_ppn=3, PTE = (3<<10)|PTE_V = 0xC01
        mem.write(Addr(0x2000), WordT(0xC01u));

        // leaf_pt[0]: VA 0x0000 -> PA 0x0000, PTE = 0*|V|R|W|X = 0x000F
        mem.write(Addr(0x3000), WordT(0x000Fu));
        // leaf_pt[1]: VA 0x1000 -> PA 0x1000, ppn=1, PTE = (1<<10)|V|R|W|X = 0x040F
        mem.write(Addr(0x3004), WordT(0x040Fu));
        // leaf_pt[2]: VA 0x2000 -> PA 0x2000, ppn=2, PTE = (2<<10)|V|R|W = 0x0807
        mem.write(Addr(0x3008), WordT(0x0807u));
        // leaf_pt[3]: VA 0x3000 -> PA 0x3000, ppn=3, PTE = (3<<10)|V|R|W = 0x0C07
        mem.write(Addr(0x300C), WordT(0x0C07u));

        // satp: Sv32 mode (bit 31) | PPN of root = 2 -> 0x80000002
        cpu.csr().write(CSR::SATP, 0x80000002u);

        loadProgram(mem,
                    {U(0x1000, 10, OP_LUI),  // x10 = 0x1000 (data VA)
                     ADDI(11, 0, 0x42),      // x11 = 0x42
                     SW(11, 10, 0),          // mem[VA 0x1000] = 0x42
                     LW(12, 10, 0),          // x12 = mem[VA 0x1000]
                     HALT()});
        cpu.run();
        check("Sv32 identity SW/LW", cpu.regs().get(12) == 0x42u);
    }

    // Sv32 load from unmapped page - page fault halts CPU (mtvec=0)
    {
        MemoryModel<32> mem(0x4000);
        RVModel<32>     cpu(Config{}, mem);

        // root_pt[0] -> leaf at 0x3000
        mem.write(Addr(0x2000), WordT(0xC01u));
        // leaf_pt[0]: VA 0x0000 mapped (code page)
        mem.write(Addr(0x3000), WordT(0x000Fu));
        // leaf_pt[1]: VA 0x1000 NOT mapped (PTE_V=0)

        cpu.csr().write(CSR::SATP, 0x80000002u);
        loadProgram(mem,
                    {U(0x1000, 10, OP_LUI),  // x10 = 0x1000 (unmapped)
                     LW(11, 10, 0),          // LW from unmapped VA -> page fault -> halt
                     HALT()});
        cpu.run();
        check("Sv32 page fault halts CPU", cpu.isHalted());
    }

    // Sv32 page fault with mtvec set - mcause = EXC_LOAD_PAGE_FAULT
    {
        MemoryModel<32> mem(0x4000);
        RVModel<32>     cpu(Config{}, mem);

        // root_pt[0] -> leaf at 0x3000
        mem.write(Addr(0x2000), WordT(0xC01u));
        // leaf_pt[0]: VA 0x0000 mapped (code page only)
        mem.write(Addr(0x3000), WordT(0x000Fu));
        // leaf_pt[1]: VA 0x1000 NOT mapped

        // trap handler at 0x40: HALT
        loadProgram(mem, {HALT()}, Addr(0x40));
        cpu.csr().write(CSR::SATP, 0x80000002u);

        loadProgram(mem,
                    {ADDI(10, 0, 0x40),         // x10 = trap address
                     CSRRW(0, CSR::MTVEC, 10),  // mtvec = 0x40
                     U(0x1000, 11, OP_LUI),     // x11 = 0x1000 (unmapped)
                     LW(12, 11, 0),             // LW from unmapped VA -> page fault trap
                     HALT()});
        cpu.run();
        check("Sv32 page fault mcause", cpu.csr().getMCAUSE() == CSR::EXC_LOAD_PAGE_FAULT);
        check("Sv32 page fault mtval", cpu.csr().getMTVAL() == 0x1000u);
    }
}

static void test_csr() {
    std::cout << "\n[ CSR ]\n";
    using namespace InstrBuilder;

    // CSRRW: write rs1 to CSR, rd = old value
    {
        Fixture f({ADDI(10, 0, 42),
                   CSRRW(11, CSR::MSCRATCH, 10),  // scratch=42, rd=0 (old)
                   CSRRW(12, CSR::MSCRATCH, 0),   // scratch=0, rd=42
                   HALT()});
        f.cpu.run();
        check("CSRRW old value 0", f.cpu.regs().get(11) == 0u);
        check("CSRRW read back 42", f.cpu.regs().get(12) == 42u);
    }

    // CSRRS: set bits; rs1=x0 → no write
    {
        Fixture f({ADDI(10, 0, 0b1010),
                   CSRRW(0, CSR::MSCRATCH, 10),  // scratch = 0b1010
                   ADDI(11, 0, 0b0101),
                   CSRRS(12, CSR::MSCRATCH, 11),  // rd=0b1010, scratch=0b1111
                   CSRRS(13, CSR::MSCRATCH, 0),   // rs1=x0 → no write, rd=0b1111
                   HALT()});
        f.cpu.run();
        check("CSRRS old value", f.cpu.regs().get(12) == 0b1010u);
        check("CSRRS set bits", f.cpu.regs().get(13) == 0b1111u);
    }

    // CSRRC: clear bits; rs1=x0 → no write
    {
        Fixture f({ADDI(10, 0, 0b1111),
                   CSRRW(0, CSR::MSCRATCH, 10),  // scratch = 0b1111
                   ADDI(11, 0, 0b0101),
                   CSRRC(12, CSR::MSCRATCH, 11),  // rd=0b1111, scratch=0b1010
                   CSRRC(13, CSR::MSCRATCH, 0),   // rs1=x0 → no write, rd=0b1010
                   HALT()});
        f.cpu.run();
        check("CSRRC old value", f.cpu.regs().get(12) == 0b1111u);
        check("CSRRC clear bits", f.cpu.regs().get(13) == 0b1010u);
    }

    // CSRRWI: write immediate
    {
        Fixture f({CSRRWI(10, CSR::MSCRATCH, 7),  // scratch=7, rd=0
                   CSRRWI(11, CSR::MSCRATCH, 0),  // scratch=0, rd=7
                   HALT()});
        f.cpu.run();
        check("CSRRWI old 0", f.cpu.regs().get(10) == 0u);
        check("CSRRWI read back 7", f.cpu.regs().get(11) == 7u);
    }

    // CSRRSI: set bits from zimm; zimm=0 → no write
    {
        Fixture f({CSRRWI(0, CSR::MSCRATCH, 0b1010),
                   CSRRSI(10, CSR::MSCRATCH, 0b0101),  // rd=0b1010, scratch=0b1111
                   CSRRSI(11, CSR::MSCRATCH, 0),       // zimm=0 → no write, rd=0b1111
                   HALT()});
        f.cpu.run();
        check("CSRRSI old value", f.cpu.regs().get(10) == 0b1010u);
        check("CSRRSI set bits", f.cpu.regs().get(11) == 0b1111u);
    }

    // CSRRCI: clear bits from zimm; zimm=0 → no write
    {
        Fixture f({CSRRWI(0, CSR::MSCRATCH, 0b1111),
                   CSRRCI(10, CSR::MSCRATCH, 0b0101),  // rd=0b1111, scratch=0b1010
                   CSRRCI(11, CSR::MSCRATCH, 0),       // zimm=0 → no write, rd=0b1010
                   HALT()});
        f.cpu.run();
        check("CSRRCI old value", f.cpu.regs().get(10) == 0b1111u);
        check("CSRRCI clear bits", f.cpu.regs().get(11) == 0b1010u);
    }

    // read-only CSR: write to MHARTID (addr[11:10]=11) is no-op
    {
        Fixture f({ADDI(10, 0, 42),
                   CSRRW(0, CSR::MHARTID, 10),  // write → no-op (read-only)
                   CSRRS(11, CSR::MHARTID, 0),  // read
                   HALT()});
        f.cpu.run();
        check("read-only CSR stays 0", f.cpu.regs().get(11) == 0u);
    }
}

static void test_config() {
    std::cout << "\n[ Config ]\n";

    Config c(Config::EXT_M);
    check("EXT_M recognized", c.hasExtension(Config::EXT_M));
    check("EXT_A not set", !c.hasExtension(Config::EXT_A));
    check("EXT_A valid", Config(Config::EXT_A).hasExtension(Config::EXT_A));
    // EXT_F: assert fires in Debug; nothing to check via CHECK_THROWS in Release
}

void test_disasm() {
    using namespace InstrBuilder;
    using namespace Disasm;
    auto dis = [](Word w) { return disassemble(Decoder<>::decode(w)); };

    std::cout << "\n[ Disasm ]\n";
    check("addi", dis(ADDI(10, 0, 42)) == "addi a0, zero, 42");
    check("add", dis(ADD(10, 11, 12)) == "add a0, a1, a2");
    check("sub", dis(SUB(10, 11, 12)) == "sub a0, a1, a2");
    check("lui", dis(LUI(10, 1)) == "lui a0, 1");
    check("auipc", dis(AUIPC(10, 1)) == "auipc a0, 1");
    check("lw", dis(LW(10, 2, 0)) == "lw a0, 0(sp)");
    check("sw", dis(SW(10, 2, 4)) == "sw a0, 4(sp)");
    check("beq", dis(BEQ(10, 11, 8)) == "beq a0, a1, 8");
    check("jal", dis(JAL(1, 16)) == "jal ra, 16");
    check("mul", dis(MUL(10, 11, 12)) == "mul a0, a1, a2");
    check("div", dis(DIV(10, 11, 12)) == "div a0, a1, a2");
    check("ecall", dis(ECALL()) == "ecall");
    check("csrrs", dis(CSRRS(10, 0x341, 0)) == "csrrs a0, mepc, zero");
    check("slli", dis(SLLI(10, 10, 2)) == "slli a0, a0, 2");
}

void test_mmio() {
    std::cout << "\n[ MMIO ]\n";

    MemoryModel<> mem(4096);

    // word register: writable, readable
    uint32_t mmio_reg = 0;
    mem.mapMmio(
        0x1000,
        4,
        [&](uint32_t) { return mmio_reg; },
        [&](uint32_t, uint32_t val) { mmio_reg = val; });

    mem.write(uint32_t(0x1000), uint32_t(0xDEADBEEFu));
    check("mmio word write", mmio_reg == 0xDEADBEEFu);
    check("mmio word read", mem.readWord(0x1000u) == 0xDEADBEEFu);

    // byte write into mmio (read-modify-write on word register)
    mmio_reg = 0x12345678u;
    mem.write(uint32_t(0x1000), uint8_t(0xAAu));  // overwrite byte 0
    check("mmio byte write", mmio_reg == 0x123456AAu);
    check("mmio byte read", mem.readByte(0x1001u) == 0x56u);

    // half write
    mmio_reg = 0x12345678u;
    mem.write(uint32_t(0x1000), uint16_t(0xBBCCu));
    check("mmio half write", mmio_reg == 0x1234BBCCu);
    check("mmio half read", mem.readHalf(0x1002u) == 0x1234u);

    // ram below/above mmio is unaffected
    mem.write(uint32_t(0x0FF0), uint32_t(0x42u));
    check("ram below mmio intact", mem.readWord(0x0FF0u) == 0x42u);

    // ecall-style mmio: capture written chars
    std::string output;
    mem.mapMmio(
        0x2000,
        4,
        [](uint32_t) { return 0u; },
        [&](uint32_t, uint32_t val) { output += static_cast<char>(val & 0xFFu); });

    mem.write(uint32_t(0x2000), uint32_t('H'));
    mem.write(uint32_t(0x2000), uint32_t('i'));
    check("mmio uart sim", output == "Hi");
}

int main() {
    std::cout << "===========================================\n";
    std::cout << "           RV32I CPU — Test Suite          \n";
    std::cout << "===========================================\n";

    test_memory();
    test_register_file();
    test_alu();
    test_decoder();
    test_cpu();
    test_alignment();
    test_vmem();
    test_csr();
    test_config();
    test_disasm();
    test_mmio();

    std::cout << "\n===========================================\n";
    std::cout << "  " << passed << " passed,  " << failed << " failed\n";
    std::cout << "===========================================\n";
    return (failed == 0) ? 0 : 1;
}