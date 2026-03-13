#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../include/alu.hpp"
#include "../include/config.hpp"
#include "../include/decoder.hpp"
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

    // Division corner cases (RISC-V spec)
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
    // B-type negative offset
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

    // LB sign extension
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

    // BEQ taken — skips one instruction
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

    // JAL skips instruction, sets ra
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

    // M extension: MUL / DIV / REM
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

    CHECK_THROWS("M ext disabled throws",
                 std::runtime_error,
                 Fixture({InstrBuilder::ADDI(10, 0, 7),
                          InstrBuilder::ADDI(11, 0, 6),
                          InstrBuilder::MUL(12, 10, 11),
                          InstrBuilder::HALT()},
                         Config::EXT_NONE)
                     .cpu.run());

    CHECK_THROWS(
        "illegal opcode throws", std::runtime_error, Fixture({Word(0xFFFFFFFFu)}).cpu.run());

    // init() resets state — run twice on same CPU
    {
        using namespace InstrBuilder;
        MemoryModel mem(4096);
        loadProgram(mem, {ADDI(10, 0, 7), HALT()});
        RVModel cpu(Config{}, mem);
        cpu.run();
        check("first run result", cpu.regs().get(10) == 7u);
        cpu.init(0);
        cpu.run();
        check("init() resets and reruns", cpu.regs().get(10) == 7u);
    }

    // getPC() reflects current PC
    {
        using namespace InstrBuilder;
        MemoryModel mem(4096);
        loadProgram(mem, {ADDI(10, 0, 1), HALT()});
        RVModel cpu(Config{}, mem);
        check("PC starts at 0", cpu.getPC() == 0u);
        cpu.execute();
        check("PC advances to 4", cpu.getPC() == 4u);
    }
}

static void test_config() {
    std::cout << "\n[ Config ]\n";

    Config c(Config::EXT_M);
    check("EXT_M recognized", c.hasExtension(Config::EXT_M));
    check("EXT_A not set", !c.hasExtension(Config::EXT_A));
    CHECK_THROWS("EXT_A throws", std::invalid_argument, (Config(Config::EXT_A)));
    CHECK_THROWS("EXT_F throws", std::invalid_argument, (Config(Config::EXT_F)));
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
    test_config();

    std::cout << "\n===========================================\n";
    std::cout << "  " << passed << " passed,  " << failed << " failed\n";
    std::cout << "===========================================\n";
    return (failed == 0) ? 0 : 1;
}