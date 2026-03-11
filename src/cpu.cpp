#include "../include/cpu.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "../include/alu.hpp"
#include "../include/isa.hpp"

CPU::CPU(Config cfg, Memory& mem)
    : config_(cfg), mem_(mem), halted_(false), instrCount_(0), debugMode_(false) {
    regs_.setPC(0u);
}

CPU::CPU(CPU&& other) noexcept
    : config_(other.config_),
      mem_(other.mem_)  // reference stays the same
      ,
      regs_(std::move(other.regs_)),
      halted_(other.halted_),
      instrCount_(other.instrCount_),
      debugMode_(other.debugMode_) {
    // leave moved-from in halted state
    other.halted_ = true;
}

void CPU::setPC(Word addr) {
    assert(!halted_ && "CPU::setPC — CPU is halted");
    regs_.setPC(addr);
}

void CPU::execute() {
    assert(!halted_ && "CPU::execute — called on halted CPU");

    if (halted_)
        return;

    const Word pc = regs_.getPC();

    Word raw;
    try {
        raw = mem_.loadWord(pc);
    } catch (const std::out_of_range& e) {
        throw std::runtime_error(std::string("CPU: fetch fault at PC=0x") + std::to_string(pc) +
                                 ": " + e.what());
    }

    if (raw == 0u) {
        if (debugMode_)
            std::cout << "[CPU] HALT sentinel at PC=0x" << std::hex << pc << std::dec << "\n";
        halted_ = true;
        return;
    }

    const DecodedInstr instr = Decoder::decode(raw);
    if (debugMode_)
        std::cout << "[PC=0x" << std::hex << std::setw(8) << std::setfill('0') << pc << std::dec
                  << "] " << instr.toString() << "\n";

    const bool pcModified = executeInstr(instr);

    if (!pcModified)
        regs_.advancePC();

    ++instrCount_;
}

void CPU::run(uint64_t maxSteps) {
    uint64_t steps = 0;
    while (!halted_) {
        execute();
        if (maxSteps > 0 && ++steps >= maxSteps)
            break;
    }
    if (debugMode_)
        std::cout << "[CPU] Executed " << instrCount_ << " instructions.\n";
}

bool CPU::executeInstr(const DecodedInstr& d) {
    using namespace ISA;

    switch (d.opcode) {

        // LUI: rd = imm[31:12]
        case OP_LUI:
            regs_.write(d.rd, static_cast<Word>(d.imm));
            return false;

        // AUIPC: rd = PC + imm[31:12]
        case OP_AUIPC:
            regs_.write(d.rd, regs_.getPC() + static_cast<Word>(d.imm));
            return false;

        // JAL: rd = PC+4 ; PC = PC + imm
        case OP_JAL: {
            const Word ret    = regs_.getPC() + 4u;
            const Word target = regs_.getPC() + static_cast<Word>(d.imm);
            // [ASSERT] JAL target must be 4-byte aligned
            assert((target & 0x3u) == 0 && "JAL: target not 4-byte aligned");
            regs_.write(d.rd, ret);
            regs_.setPC(target);
            return true;
        }

        // JALR: rd = PC+4 ; PC = (rs1 + imm) & ~1
        case OP_JALR: {
            const Word ret    = regs_.getPC() + 4u;
            const Word target = (regs_.read(d.rs1) + static_cast<Word>(d.imm)) & ~1u;
            regs_.write(d.rd, ret);
            regs_.setPC(target);
            return true;
        }

        case OP_BRANCH:
            return executeBranch(d);
        case OP_LOAD:
            return executeLoad(d);
        case OP_STORE:
            return executeStore(d);
        case OP_OP_IMM:
            return executeOpImm(d);
        case OP_OP:
            return executeOp(d);

        // FENCE — NOP in single-threaded model without cache
        case OP_MISC_MEM:
            return false;

        // SYSTEM (ECALL/EBREAK) — not implemented, halt
        case OP_SYSTEM:
            if (debugMode_)
                std::cout << "[CPU] SYSTEM at PC=0x" << std::hex << regs_.getPC() << " — HALTED\n";
            halted_ = true;
            return true;

        default:
            throw std::runtime_error("CPU: illegal opcode 0x" + std::to_string(d.opcode) +
                                     " at PC=0x" + std::to_string(regs_.getPC()));
    }
}

bool CPU::executeBranch(const DecodedInstr& d) {
    using namespace ISA;
    const Word  r1 = regs_.read(d.rs1), r2 = regs_.read(d.rs2);
    const SWord s1 = static_cast<SWord>(r1), s2 = static_cast<SWord>(r2);

    bool taken = false;
    switch (d.funct3) {
        case F3_BEQ:
            taken = (r1 == r2);
            break;
        case F3_BNE:
            taken = (r1 != r2);
            break;
        case F3_BLT:
            taken = (s1 < s2);
            break;
        case F3_BGE:
            taken = (s1 >= s2);
            break;
        case F3_BLTU:
            taken = (r1 < r2);
            break;
        case F3_BGEU:
            taken = (r1 >= r2);
            break;
        default:
            throw std::runtime_error("CPU: unknown branch funct3=0x" + std::to_string(d.funct3));
    }

    if (taken) {
        const Word target = regs_.getPC() + static_cast<Word>(d.imm);
        assert((target & 0x3u) == 0 && "BRANCH: target not 4-byte aligned");
        regs_.setPC(target);
        return true;
    }
    return false;
}

bool CPU::executeLoad(const DecodedInstr& d) {
    using namespace ISA;
    const Addr addr = static_cast<Addr>(static_cast<SWord>(regs_.read(d.rs1)) + d.imm);

    Word result = 0u;
    try {
        switch (d.funct3) {
            case F3_LB:
                result = static_cast<Word>(ISA::signExtend(mem_.loadByte(addr), 8));
                break;
            case F3_LH:
                result = static_cast<Word>(ISA::signExtend(mem_.loadHalf(addr), 16));
                break;
            case F3_LW:
                result = mem_.loadWord(addr);
                break;
            case F3_LBU:
                result = static_cast<Word>(mem_.loadByte(addr));
                break;
            case F3_LHU:
                result = static_cast<Word>(mem_.loadHalf(addr));
                break;
            default:
                throw std::runtime_error("CPU: unknown load funct3=0x" + std::to_string(d.funct3));
        }
    } catch (const std::out_of_range& e) {
        throw std::runtime_error(std::string("CPU: load fault at addr=0x") + std::to_string(addr) +
                                 ": " + e.what());
    }

    regs_.write(d.rd, result);
    return false;
}

bool CPU::executeStore(const DecodedInstr& d) {
    using namespace ISA;
    const Addr addr = static_cast<Addr>(static_cast<SWord>(regs_.read(d.rs1)) + d.imm);
    const Word data = regs_.read(d.rs2);

    try {
        switch (d.funct3) {
            case F3_SB:
                mem_.storeByte(addr, static_cast<uint8_t>(data & 0xFFu));
                break;
            case F3_SH:
                mem_.storeHalf(addr, static_cast<uint16_t>(data & 0xFFFFu));
                break;
            case F3_SW:
                mem_.storeWord(addr, data);
                break;
            default:
                throw std::runtime_error("CPU: unknown store funct3=0x" + std::to_string(d.funct3));
        }
    } catch (const std::out_of_range& e) {
        throw std::runtime_error(std::string("CPU: store fault at addr=0x") + std::to_string(addr) +
                                 ": " + e.what());
    }
    return false;
}

bool CPU::executeOpImm(const DecodedInstr& d) {
    using namespace ISA;
    const Word rs1v   = regs_.read(d.rs1);
    const Word immv   = static_cast<Word>(d.imm);
    Word       result = 0u;

    switch (d.funct3) {
        case F3_ADD_SUB:
            result = ALU::execute(ALU::Op::ADD, rs1v, immv);
            break;
        case F3_SLL:
            result = ALU::execute(ALU::Op::SLL, rs1v, immv & 0x1Fu);
            break;
        case F3_SLT:
            result = ALU::execute(ALU::Op::SLT, rs1v, immv);
            break;
        case F3_SLTU:
            result = ALU::execute(ALU::Op::SLTU, rs1v, immv);
            break;
        case F3_XOR:
            result = ALU::execute(ALU::Op::XOR, rs1v, immv);
            break;
        case F3_SRL_SRA:
            result =
                ALU::execute((d.funct7 & 0x20u) ? ALU::Op::SRA : ALU::Op::SRL, rs1v, immv & 0x1Fu);
            break;
        case F3_OR:
            result = ALU::execute(ALU::Op::OR, rs1v, immv);
            break;
        case F3_AND:
            result = ALU::execute(ALU::Op::AND, rs1v, immv);
            break;
        default:
            throw std::runtime_error("CPU: unknown OP_IMM funct3=0x" + std::to_string(d.funct3));
    }

    regs_.write(d.rd, result);
    return false;
}

bool CPU::executeOp(const DecodedInstr& d) {
    using namespace ISA;
    const Word rs1v   = regs_.read(d.rs1);
    const Word rs2v   = regs_.read(d.rs2);
    Word       result = 0u;

    if (d.funct7 == F7_MEXT) {
        if (!config_.hasExtension(Config::EXT_M)) {
            throw std::runtime_error("CPU: M-extension instruction at PC=0x" +
                                     std::to_string(regs_.getPC()) +
                                     " but EXT_M is disabled in Config");
        }
        switch (d.funct3) {
            case F3_MUL:
                result = ALU::execute(ALU::Op::MUL, rs1v, rs2v);
                break;
            case F3_MULH:
                result = ALU::execute(ALU::Op::MULH, rs1v, rs2v);
                break;
            case F3_MULHSU:
                result = ALU::execute(ALU::Op::MULHSU, rs1v, rs2v);
                break;
            case F3_MULHU:
                result = ALU::execute(ALU::Op::MULHU, rs1v, rs2v);
                break;
            case F3_DIV:
                result = ALU::execute(ALU::Op::DIV, rs1v, rs2v);
                break;
            case F3_DIVU:
                result = ALU::execute(ALU::Op::DIVU, rs1v, rs2v);
                break;
            case F3_REM:
                result = ALU::execute(ALU::Op::REM, rs1v, rs2v);
                break;
            case F3_REMU:
                result = ALU::execute(ALU::Op::REMU, rs1v, rs2v);
                break;
            default:
                throw std::runtime_error("CPU: unknown M-ext funct3=0x" + std::to_string(d.funct3));
        }
    } else {
        switch (d.funct3) {
            case F3_ADD_SUB:
                result =
                    ALU::execute((d.funct7 == F7_ALT) ? ALU::Op::SUB : ALU::Op::ADD, rs1v, rs2v);
                break;
            case F3_SLL:
                result = ALU::execute(ALU::Op::SLL, rs1v, rs2v);
                break;
            case F3_SLT:
                result = ALU::execute(ALU::Op::SLT, rs1v, rs2v);
                break;
            case F3_SLTU:
                result = ALU::execute(ALU::Op::SLTU, rs1v, rs2v);
                break;
            case F3_XOR:
                result = ALU::execute(ALU::Op::XOR, rs1v, rs2v);
                break;
            case F3_SRL_SRA:
                result =
                    ALU::execute((d.funct7 == F7_ALT) ? ALU::Op::SRA : ALU::Op::SRL, rs1v, rs2v);
                break;
            case F3_OR:
                result = ALU::execute(ALU::Op::OR, rs1v, rs2v);
                break;
            case F3_AND:
                result = ALU::execute(ALU::Op::AND, rs1v, rs2v);
                break;
            default:
                throw std::runtime_error("CPU: unknown OP funct3=0x" + std::to_string(d.funct3));
        }
    }

    regs_.write(d.rd, result);
    return false;
}

Register& CPU::regs() {
    return regs_;
}
bool CPU::isHalted() const {
    return halted_;
}
uint64_t CPU::instrCount() const {
    return instrCount_;
}
void CPU::setDebug(bool on) {
    debugMode_ = on;
}
void CPU::setStackPointer(Word sp) {
    regs_.write(2, sp);
}

void CPU::reset() {
    regs_       = Register();
    halted_     = false;
    instrCount_ = 0;
    debugMode_  = false;
}