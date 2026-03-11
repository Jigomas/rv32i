#include "../include/rv_model.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "../include/alu.hpp"
#include "../include/isa.hpp"

RVModel::RVModel(Config cfg, MemoryModel& mem)
    : pc_(0u)
    , config_(cfg)
    , mem_(mem)
    , halted_(false)
    , instrCount_(0)
    , debugMode_(false) {}

RVModel::RVModel(RVModel&& other) noexcept
    : pc_(other.pc_)
    , config_(other.config_)
    , mem_(other.mem_)
    , regs_(std::move(other.regs_))
    , halted_(other.halted_)
    , instrCount_(other.instrCount_)
    , debugMode_(other.debugMode_) {
    other.halted_ = true;
}

void RVModel::init(Addr startPC, Word stackPointer) {
    regs_       = RegisterFile();
    pc_         = startPC;
    halted_     = false;
    instrCount_ = 0;
    if (stackPointer != 0)
        regs_.set(2, stackPointer);  // x2 = sp
}

void RVModel::execute() {
    assert(!halted_ && "RVModel::execute — called on halted CPU");
    if (halted_)
        return;

    // --- Fetch ---
    Word raw;
    try {
        raw = mem_.readWord(pc_);
    } catch (const std::out_of_range& e) {
        throw std::runtime_error(
            "RVModel: fetch fault at PC=0x" + std::to_string(pc_) + ": " + e.what());
    }

    // Halt sentinel: instruction word 0x00000000
    if (raw == 0u) {
        if (debugMode_)
            std::cout << "[RVModel] HALT at PC=0x" << std::hex << pc_ << std::dec << "\n";
        halted_ = true;
        return;
    }

    // --- Decode ---
    const DecodedInstr instr = Decoder::decode(raw);
    if (debugMode_)
        std::cout << "[PC=0x" << std::hex << std::setw(8) << std::setfill('0') << pc_ << std::dec
                  << "] " << instr.toString() << "\n";

    // --- Execute ---
    const bool pcModified = executeInstr(instr);
    if (!pcModified)
        advancePC();

    ++instrCount_;
}

void RVModel::run(uint64_t maxSteps) {
    uint64_t steps = 0;
    while (!halted_) {
        execute();
        if (maxSteps > 0 && ++steps >= maxSteps)
            break;
    }
    if (debugMode_)
        std::cout << "[RVModel] Executed " << instrCount_ << " instructions.\n";
}

void RVModel::setPC(Addr addr) {
    assert((addr & 0x3u) == 0 && "RVModel::setPC — address must be 4-byte aligned");
    pc_ = addr;
}

bool RVModel::executeInstr(const DecodedInstr& d) {
    using namespace ISA;

    switch (d.opcode) {

        case OP_LUI:
            regs_.set(d.rd, static_cast<Word>(d.imm));
            return false;

        case OP_AUIPC:
            regs_.set(d.rd, pc_ + static_cast<Word>(d.imm));
            return false;

        case OP_JAL: {
            const Word ret    = pc_ + 4u;
            const Word target = pc_ + static_cast<Word>(d.imm);
            assert((target & 0x3u) == 0 && "JAL: target not 4-byte aligned");
            regs_.set(d.rd, ret);
            pc_ = target;
            return true;
        }

        case OP_JALR: {
            const Word ret    = pc_ + 4u;
            const Word target = (regs_.get(d.rs1) + static_cast<Word>(d.imm)) & ~1u;
            regs_.set(d.rd, ret);
            pc_ = target;
            return true;
        }

        case OP_BRANCH:  return executeBranch(d);
        case OP_LOAD:    return executeLoad(d);
        case OP_STORE:   return executeStore(d);
        case OP_OP_IMM:  return executeOpImm(d);
        case OP_OP:      return executeOp(d);

        case OP_MISC_MEM:  // FENCE — NOP in single-threaded model
            return false;

        case OP_SYSTEM:  // ECALL / EBREAK — halt
            if (debugMode_)
                std::cout << "[RVModel] SYSTEM at PC=0x" << std::hex << pc_ << " — HALTED\n";
            halted_ = true;
            return true;

        default:
            throw std::runtime_error("RVModel: illegal opcode 0x" + std::to_string(d.opcode) +
                                     " at PC=0x" + std::to_string(pc_));
    }
}

bool RVModel::executeBranch(const DecodedInstr& d) {
    using namespace ISA;
    const Word  r1 = regs_.get(d.rs1), r2 = regs_.get(d.rs2);
    const SWord s1 = static_cast<SWord>(r1), s2 = static_cast<SWord>(r2);

    bool taken = false;
    switch (d.funct3) {
        case F3_BEQ:  taken = (r1 == r2); break;
        case F3_BNE:  taken = (r1 != r2); break;
        case F3_BLT:  taken = (s1 < s2);  break;
        case F3_BGE:  taken = (s1 >= s2); break;
        case F3_BLTU: taken = (r1 < r2);  break;
        case F3_BGEU: taken = (r1 >= r2); break;
        default:
            throw std::runtime_error("RVModel: unknown branch funct3=0x" +
                                     std::to_string(d.funct3));
    }

    if (taken) {
        const Word target = pc_ + static_cast<Word>(d.imm);
        assert((target & 0x3u) == 0 && "BRANCH: target not 4-byte aligned");
        pc_ = target;
        return true;
    }
    return false;
}

bool RVModel::executeLoad(const DecodedInstr& d) {
    using namespace ISA;
    const Addr addr = static_cast<Addr>(static_cast<SWord>(regs_.get(d.rs1)) + d.imm);

    Word result = 0u;
    try {
        switch (d.funct3) {
            case F3_LB:  result = static_cast<Word>(ISA::signExtend(mem_.readByte(addr), 8));  break;
            case F3_LH:  result = static_cast<Word>(ISA::signExtend(mem_.readHalf(addr), 16)); break;
            case F3_LW:  result = mem_.readWord(addr);                                          break;
            case F3_LBU: result = static_cast<Word>(mem_.readByte(addr));                       break;
            case F3_LHU: result = static_cast<Word>(mem_.readHalf(addr));                       break;
            default:
                throw std::runtime_error("RVModel: unknown load funct3=0x" +
                                         std::to_string(d.funct3));
        }
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("RVModel: load fault addr=0x" + std::to_string(addr) + ": " +
                                 e.what());
    }

    regs_.set(d.rd, result);
    return false;
}

bool RVModel::executeStore(const DecodedInstr& d) {
    using namespace ISA;
    const Addr addr = static_cast<Addr>(static_cast<SWord>(regs_.get(d.rs1)) + d.imm);
    const Word data = regs_.get(d.rs2);

    try {
        switch (d.funct3) {
            case F3_SB: mem_.write(addr, static_cast<ByteT>(data & 0xFFu));   break;
            case F3_SH: mem_.write(addr, static_cast<HalfT>(data & 0xFFFFu)); break;
            case F3_SW: mem_.write(addr, static_cast<WordT>(data));            break;
            default:
                throw std::runtime_error("RVModel: unknown store funct3=0x" +
                                         std::to_string(d.funct3));
        }
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("RVModel: store fault addr=0x" + std::to_string(addr) + ": " +
                                 e.what());
    }
    return false;
}

bool RVModel::executeOpImm(const DecodedInstr& d) {
    using namespace ISA;
    const Word rs1v   = regs_.get(d.rs1);
    const Word immv   = static_cast<Word>(d.imm);
    Word       result = 0u;

    switch (d.funct3) {
        case F3_ADD_SUB: result = ALU::execute(ALU::Op::ADD,  rs1v, immv);            break;
        case F3_SLL:     result = ALU::execute(ALU::Op::SLL,  rs1v, immv & 0x1Fu);   break;
        case F3_SLT:     result = ALU::execute(ALU::Op::SLT,  rs1v, immv);            break;
        case F3_SLTU:    result = ALU::execute(ALU::Op::SLTU, rs1v, immv);            break;
        case F3_XOR:     result = ALU::execute(ALU::Op::XOR,  rs1v, immv);            break;
        case F3_SRL_SRA:
            result = ALU::execute(
                (d.funct7 & 0x20u) ? ALU::Op::SRA : ALU::Op::SRL, rs1v, immv & 0x1Fu);
            break;
        case F3_OR:  result = ALU::execute(ALU::Op::OR,  rs1v, immv); break;
        case F3_AND: result = ALU::execute(ALU::Op::AND, rs1v, immv); break;
        default:
            throw std::runtime_error("RVModel: unknown OP_IMM funct3=0x" +
                                     std::to_string(d.funct3));
    }

    regs_.set(d.rd, result);
    return false;
}

bool RVModel::executeOp(const DecodedInstr& d) {
    using namespace ISA;
    const Word rs1v   = regs_.get(d.rs1);
    const Word rs2v   = regs_.get(d.rs2);
    Word       result = 0u;

    if (d.funct7 == F7_MEXT) {
        if (!config_.hasExtension(Config::EXT_M))
            throw std::runtime_error(
                "RVModel: M-extension disabled at PC=0x" + std::to_string(pc_));
        switch (d.funct3) {
            case F3_MUL:    result = ALU::execute(ALU::Op::MUL,    rs1v, rs2v); break;
            case F3_MULH:   result = ALU::execute(ALU::Op::MULH,   rs1v, rs2v); break;
            case F3_MULHSU: result = ALU::execute(ALU::Op::MULHSU, rs1v, rs2v); break;
            case F3_MULHU:  result = ALU::execute(ALU::Op::MULHU,  rs1v, rs2v); break;
            case F3_DIV:    result = ALU::execute(ALU::Op::DIV,    rs1v, rs2v); break;
            case F3_DIVU:   result = ALU::execute(ALU::Op::DIVU,   rs1v, rs2v); break;
            case F3_REM:    result = ALU::execute(ALU::Op::REM,    rs1v, rs2v); break;
            case F3_REMU:   result = ALU::execute(ALU::Op::REMU,   rs1v, rs2v); break;
            default:
                throw std::runtime_error("RVModel: unknown M-ext funct3=0x" +
                                         std::to_string(d.funct3));
        }
    } else {
        switch (d.funct3) {
            case F3_ADD_SUB:
                result = ALU::execute(
                    (d.funct7 == F7_ALT) ? ALU::Op::SUB : ALU::Op::ADD, rs1v, rs2v);
                break;
            case F3_SLL:  result = ALU::execute(ALU::Op::SLL,  rs1v, rs2v); break;
            case F3_SLT:  result = ALU::execute(ALU::Op::SLT,  rs1v, rs2v); break;
            case F3_SLTU: result = ALU::execute(ALU::Op::SLTU, rs1v, rs2v); break;
            case F3_XOR:  result = ALU::execute(ALU::Op::XOR,  rs1v, rs2v); break;
            case F3_SRL_SRA:
                result = ALU::execute(
                    (d.funct7 == F7_ALT) ? ALU::Op::SRA : ALU::Op::SRL, rs1v, rs2v);
                break;
            case F3_OR:  result = ALU::execute(ALU::Op::OR,  rs1v, rs2v); break;
            case F3_AND: result = ALU::execute(ALU::Op::AND, rs1v, rs2v); break;
            default:
                throw std::runtime_error("RVModel: unknown OP funct3=0x" +
                                         std::to_string(d.funct3));
        }
    }

    regs_.set(d.rd, result);
    return false;
}

RegisterFile& RVModel::regs()             { return regs_; }
bool          RVModel::isHalted() const   { return halted_; }
uint64_t      RVModel::instrCount() const { return instrCount_; }
void          RVModel::setDebug(bool on)  { debugMode_ = on; }

void RVModel::reset() {
    init(0, 0);
    debugMode_ = false;
}