#pragma once
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "alu.hpp"
#include "config.hpp"
#include "decoder.hpp"
#include "isa.hpp"
#include "memory_model.hpp"
#include "register_file.hpp"
#include "types.hpp"

template <int XLEN = 32>
class RVModel {
public:
    using UWord = typename XlenTraits<XLEN>::UWord;
    using SWord = typename XlenTraits<XLEN>::SWord;
    using Addr  = typename XlenTraits<XLEN>::Addr;

    RVModel(Config cfg, MemoryModel<XLEN>& mem)
        : pc_(UWord(0)),
          config_(cfg),
          mem_(mem),
          halted_(false),
          instrCount_(0),
          debugMode_(false) {}

    ~RVModel()                         = default;
    RVModel(const RVModel&)            = delete;
    RVModel& operator=(const RVModel&) = delete;

    RVModel(RVModel&& other) noexcept
        : pc_(other.pc_),
          config_(other.config_),
          mem_(other.mem_),
          regs_(std::move(other.regs_)),
          halted_(other.halted_),
          instrCount_(other.instrCount_),
          debugMode_(other.debugMode_) {
        other.halted_ = true;
    }

    RVModel& operator=(RVModel&&) = delete;

    void init(Addr startPC = 0, UWord stackPointer = 0) {
        regs_       = RegisterFile<XLEN>();
        pc_         = startPC;
        halted_     = false;
        instrCount_ = 0;
        if (stackPointer != UWord(0))
            regs_.set(2, stackPointer);  // x2 = sp
    }

    void step() {
        assert(!halted_ && "RVModel::step — called on halted CPU");
        assert((pc_ & UWord(0b11)) == UWord(0) && "RVModel::step — PC not 4-byte aligned");
        if (halted_)
            return;

        Word raw;
        try {
            raw = mem_.readWord(pc_);
        } catch (const std::out_of_range& e) {
            throw std::runtime_error("RVModel: fetch fault at PC=0x" + std::to_string(pc_) + ": " +
                                     e.what());
        }

        if (raw == 0u) {
            if (debugMode_)
                std::cout << "[RVModel] HALT at PC=0x" << std::hex << pc_ << std::dec << "\n";
            halted_ = true;
            return;
        }

        const DecodedInstr<XLEN> instr = Decoder<XLEN>::decode(raw);
        if (debugMode_)
            std::cout << "[PC=0x" << std::hex << std::setw(8) << std::setfill('0') << pc_
                      << std::dec << "] " << instr.toString() << "\n";

        const bool pcModified = executeInstr(instr);
        if (!pcModified)
            advancePC();

        ++instrCount_;
    }

    void run(uint64_t maxSteps = 0) {
        uint64_t steps = 0;
        while (!halted_) {
            step();
            if (maxSteps > 0 && ++steps >= maxSteps)
                break;
        }
        if (debugMode_)
            std::cout << "[RVModel] Executed " << instrCount_ << " instructions.\n";
    }

    UWord getPC() const { return pc_; }

    void setPC(Addr addr) {
        assert((addr & UWord(0b11)) == UWord(0) &&
               "RVModel::setPC — address must be 4-byte aligned");
        pc_ = addr;
    }

    RegisterFile<XLEN>& regs() { return regs_; }
    bool                isHalted() const { return halted_; }
    uint64_t            instrCount() const { return instrCount_; }
    void                setDebug(bool on) { debugMode_ = on; }
    void                reset() {
        init(0, 0);
        debugMode_ = false;
    }

private:
    UWord              pc_;
    Config             config_;
    MemoryModel<XLEN>& mem_;
    RegisterFile<XLEN> regs_;
    bool               halted_;
    uint64_t           instrCount_;
    bool               debugMode_;

    void advancePC() { pc_ += UWord(4); }

    bool executeInstr(const DecodedInstr<XLEN>& d) {
        using namespace ISA;

        switch (d.opcode) {
            case OP_LUI:
                regs_.set(d.rd, static_cast<UWord>(d.imm));
                return false;

            case OP_AUIPC:
                regs_.set(d.rd, pc_ + static_cast<UWord>(d.imm));
                return false;

            case OP_JAL: {
                const UWord ret    = pc_ + UWord(4);
                const UWord target = pc_ + static_cast<UWord>(d.imm);
                assert((target & UWord(0b11)) == UWord(0) && "JAL: target not 4-byte aligned");
                regs_.set(d.rd, ret);
                pc_ = target;
                return true;
            }

            case OP_JALR: {
                const UWord ret    = pc_ + UWord(4);
                const UWord target = (regs_.get(d.rs1) + static_cast<UWord>(d.imm)) & ~UWord(1);
                regs_.set(d.rd, ret);
                pc_ = target;
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

            case OP_MISC_MEM:
                return false;

            case OP_SYSTEM:
                if (debugMode_)
                    std::cout << "[RVModel] SYSTEM at PC=0x" << std::hex << pc_ << " — HALTED\n";
                halted_ = true;
                return true;

            default:
                throw std::runtime_error("RVModel: illegal opcode 0x" + std::to_string(d.opcode) +
                                         " at PC=0x" + std::to_string(pc_));
        }
    }

    bool executeBranch(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        const UWord r1 = regs_.get(d.rs1);
        const UWord r2 = regs_.get(d.rs2);
        const SWord s1 = static_cast<SWord>(r1);
        const SWord s2 = static_cast<SWord>(r2);

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
                throw std::runtime_error("RVModel: unknown branch funct3=0x" +
                                         std::to_string(d.funct3));
        }

        if (taken) {
            const UWord target = pc_ + static_cast<UWord>(d.imm);
            assert((target & UWord(0b11)) == UWord(0) && "BRANCH: target not 4-byte aligned");
            pc_ = target;
            return true;
        }
        return false;
    }

    bool executeLoad(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        const Addr addr = static_cast<Addr>(static_cast<SWord>(regs_.get(d.rs1)) + d.imm);

        UWord result = UWord(0);
        try {
            switch (d.funct3) {
                case F3_LB:
                    result = static_cast<UWord>(ISA::signExtend<XLEN>(mem_.readByte(addr), 8));
                    break;
                case F3_LH:
                    result = static_cast<UWord>(ISA::signExtend<XLEN>(mem_.readHalf(addr), 16));
                    break;
                case F3_LW:
                    result = static_cast<UWord>(mem_.readWord(addr));
                    break;
                case F3_LBU:
                    result = static_cast<UWord>(mem_.readByte(addr));
                    break;
                case F3_LHU:
                    result = static_cast<UWord>(mem_.readHalf(addr));
                    break;
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

    bool executeStore(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        const Addr  addr = static_cast<Addr>(static_cast<SWord>(regs_.get(d.rs1)) + d.imm);
        const UWord data = regs_.get(d.rs2);

        try {
            switch (d.funct3) {
                case F3_SB:
                    mem_.write(addr, static_cast<ByteT>(data & 0xFFu));
                    break;
                case F3_SH:
                    mem_.write(addr, static_cast<HalfT>(data & 0xFFFFu));
                    break;
                case F3_SW:
                    mem_.write(addr, static_cast<WordT>(data));
                    break;
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

    bool executeOpImm(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        using Op           = typename ALU<XLEN>::Op;
        const UWord rs1v   = regs_.get(d.rs1);
        const UWord immv   = static_cast<UWord>(d.imm);
        UWord       result = UWord(0);

        switch (d.funct3) {
            case F3_ADD_SUB:
                result = ALU<XLEN>::step(Op::ADD, rs1v, immv);
                break;
            case F3_SLL:
                result = ALU<XLEN>::step(Op::SLL, rs1v, immv);
                break;
            case F3_SLT:
                result = ALU<XLEN>::step(Op::SLT, rs1v, immv);
                break;
            case F3_SLTU:
                result = ALU<XLEN>::step(Op::SLTU, rs1v, immv);
                break;
            case F3_XOR:
                result = ALU<XLEN>::step(Op::XOR, rs1v, immv);
                break;
            case F3_SRL_SRA:
                result = ALU<XLEN>::step((d.funct7 & 0b00100000u) ? Op::SRA : Op::SRL, rs1v, immv);
                break;
            case F3_OR:
                result = ALU<XLEN>::step(Op::OR, rs1v, immv);
                break;
            case F3_AND:
                result = ALU<XLEN>::step(Op::AND, rs1v, immv);
                break;
            default:
                throw std::runtime_error("RVModel: unknown OP_IMM funct3=0x" +
                                         std::to_string(d.funct3));
        }

        regs_.set(d.rd, result);
        return false;
    }

    bool executeOp(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        using Op           = typename ALU<XLEN>::Op;
        const UWord rs1v   = regs_.get(d.rs1);
        const UWord rs2v   = regs_.get(d.rs2);
        UWord       result = UWord(0);

        if (d.funct7 == F7_MEXT) {
            if (!config_.hasExtension(Config::EXT_M))
                throw std::runtime_error("RVModel: M-extension disabled at PC=0x" +
                                         std::to_string(pc_));
            switch (d.funct3) {
                case F3_MUL:
                    result = ALU<XLEN>::step(Op::MUL, rs1v, rs2v);
                    break;
                case F3_MULH:
                    result = ALU<XLEN>::step(Op::MULH, rs1v, rs2v);
                    break;
                case F3_MULHSU:
                    result = ALU<XLEN>::step(Op::MULHSU, rs1v, rs2v);
                    break;
                case F3_MULHU:
                    result = ALU<XLEN>::step(Op::MULHU, rs1v, rs2v);
                    break;
                case F3_DIV:
                    result = ALU<XLEN>::step(Op::DIV, rs1v, rs2v);
                    break;
                case F3_DIVU:
                    result = ALU<XLEN>::step(Op::DIVU, rs1v, rs2v);
                    break;
                case F3_REM:
                    result = ALU<XLEN>::step(Op::REM, rs1v, rs2v);
                    break;
                case F3_REMU:
                    result = ALU<XLEN>::step(Op::REMU, rs1v, rs2v);
                    break;
                default:
                    throw std::runtime_error("RVModel: unknown M-ext funct3=0x" +
                                             std::to_string(d.funct3));
            }
        } else {
            switch (d.funct3) {
                case F3_ADD_SUB:
                    result =
                        ALU<XLEN>::step((d.funct7 == F7_ALT) ? Op::SUB : Op::ADD, rs1v, rs2v);
                    break;
                case F3_SLL:
                    result = ALU<XLEN>::step(Op::SLL, rs1v, rs2v);
                    break;
                case F3_SLT:
                    result = ALU<XLEN>::step(Op::SLT, rs1v, rs2v);
                    break;
                case F3_SLTU:
                    result = ALU<XLEN>::step(Op::SLTU, rs1v, rs2v);
                    break;
                case F3_XOR:
                    result = ALU<XLEN>::step(Op::XOR, rs1v, rs2v);
                    break;
                case F3_SRL_SRA:
                    result =
                        ALU<XLEN>::step((d.funct7 == F7_ALT) ? Op::SRA : Op::SRL, rs1v, rs2v);
                    break;
                case F3_OR:
                    result = ALU<XLEN>::step(Op::OR, rs1v, rs2v);
                    break;
                case F3_AND:
                    result = ALU<XLEN>::step(Op::AND, rs1v, rs2v);
                    break;
                default:
                    throw std::runtime_error("RVModel: unknown OP funct3=0x" +
                                             std::to_string(d.funct3));
            }
        }

        regs_.set(d.rd, result);
        return false;
    }
};