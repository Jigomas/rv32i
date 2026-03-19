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

// Full context: all 32 registers + pc — used on trap/timer interrupt
template <int XLEN = 32>
struct FullContext {
    typename XlenTraits<XLEN>::UWord pc       = 0;
    typename XlenTraits<XLEN>::UWord regs[32] = {};
};

// Minimal context: only callee-saved registers + sp + ra + pc
template <int XLEN = 32>
struct Context {
    typename XlenTraits<XLEN>::UWord pc  = 0;
    typename XlenTraits<XLEN>::UWord ra  = 0;  // x1
    typename XlenTraits<XLEN>::UWord sp  = 0;  // x2
    typename XlenTraits<XLEN>::UWord s0  = 0;  // x8
    typename XlenTraits<XLEN>::UWord s1  = 0;  // x9
    typename XlenTraits<XLEN>::UWord s2  = 0;  // x18
    typename XlenTraits<XLEN>::UWord s3  = 0;  // x19
    typename XlenTraits<XLEN>::UWord s4  = 0;  // x20
    typename XlenTraits<XLEN>::UWord s5  = 0;  // x21
    typename XlenTraits<XLEN>::UWord s6  = 0;  // x22
    typename XlenTraits<XLEN>::UWord s7  = 0;  // x23
    typename XlenTraits<XLEN>::UWord s8  = 0;  // x24
    typename XlenTraits<XLEN>::UWord s9  = 0;  // x25
    typename XlenTraits<XLEN>::UWord s10 = 0;  // x26
    typename XlenTraits<XLEN>::UWord s11 = 0;  // x27
};

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

    // Approach 2: save/restore only callee-saved registers + sp + ra + pc
    Context<XLEN> saveContext() const {
        Context<XLEN> ctx;
        ctx.pc  = pc_;
        ctx.ra  = regs_.get(1);
        ctx.sp  = regs_.get(2);
        ctx.s0  = regs_.get(8);
        ctx.s1  = regs_.get(9);
        ctx.s2  = regs_.get(18);
        ctx.s3  = regs_.get(19);
        ctx.s4  = regs_.get(20);
        ctx.s5  = regs_.get(21);
        ctx.s6  = regs_.get(22);
        ctx.s7  = regs_.get(23);
        ctx.s8  = regs_.get(24);
        ctx.s9  = regs_.get(25);
        ctx.s10 = regs_.get(26);
        ctx.s11 = regs_.get(27);
        return ctx;
    }

    FullContext<XLEN> saveFullContext() const {
        FullContext<XLEN> ctx;
        ctx.pc = pc_;
        for (std::size_t i = 0; i < 32; ++i)
            ctx.regs[i] = regs_.get(i);
        return ctx;
    }

    void restoreFullContext(const FullContext<XLEN>& ctx) {
        pc_ = ctx.pc;
        for (std::size_t i = 1; i < 32; ++i)
            regs_.set(i, ctx.regs[i]);
    }

    void restoreContext(const Context<XLEN>& ctx) {
        pc_ = ctx.pc;
        regs_.set(1,  ctx.ra);
        regs_.set(2,  ctx.sp);
        regs_.set(8,  ctx.s0);
        regs_.set(9,  ctx.s1);
        regs_.set(18, ctx.s2);
        regs_.set(19, ctx.s3);
        regs_.set(20, ctx.s4);
        regs_.set(21, ctx.s5);
        regs_.set(22, ctx.s6);
        regs_.set(23, ctx.s7);
        regs_.set(24, ctx.s8);
        regs_.set(25, ctx.s9);
        regs_.set(26, ctx.s10);
        regs_.set(27, ctx.s11);
    }
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

            case OP_AMO:
                return executeAMO(d);

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
        mem_.invalidateReservation();
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
                result = ALU<XLEN>::execute(Op::ADD, rs1v, immv);
                break;
            case F3_SLL:
                result = ALU<XLEN>::execute(Op::SLL, rs1v, immv);
                break;
            case F3_SLT:
                result = ALU<XLEN>::execute(Op::SLT, rs1v, immv);
                break;
            case F3_SLTU:
                result = ALU<XLEN>::execute(Op::SLTU, rs1v, immv);
                break;
            case F3_XOR:
                result = ALU<XLEN>::execute(Op::XOR, rs1v, immv);
                break;
            case F3_SRL_SRA:
                result = ALU<XLEN>::execute((d.funct7 & 0b00100000u) ? Op::SRA : Op::SRL, rs1v, immv);
                break;
            case F3_OR:
                result = ALU<XLEN>::execute(Op::OR, rs1v, immv);
                break;
            case F3_AND:
                result = ALU<XLEN>::execute(Op::AND, rs1v, immv);
                break;
            default:
                throw std::runtime_error("RVModel: unknown OP_IMM funct3=0x" +
                                         std::to_string(d.funct3));
        }

        regs_.set(d.rd, result);
        return false;
    }

    // A-extension: atomic memory operations (LR/SC + AMO)
    bool executeAMO(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        if (!config_.hasExtension(Config::EXT_A))
            throw std::runtime_error("RVModel: A-extension disabled at PC=0x" +
                                     std::to_string(pc_));

        const Addr    addr   = static_cast<Addr>(regs_.get(d.rs1));
        const UWord   rs2v   = regs_.get(d.rs2);
        const uint8_t funct5 = static_cast<uint8_t>(d.funct7 >> 2);

        if (funct5 == F5_LR) {
            regs_.set(d.rd, static_cast<UWord>(mem_.readWord(addr)));
            mem_.reserveLoad(addr);
            return false;
        }

        if (funct5 == F5_SC) {
            const bool ok = mem_.storeConditional(addr, static_cast<WordT>(rs2v));
            regs_.set(d.rd, ok ? UWord(0) : UWord(1));
            return false;
        }

        const UWord loaded  = static_cast<UWord>(mem_.readWord(addr));
        const SWord sloaded = static_cast<SWord>(loaded);
        const SWord srs2v   = static_cast<SWord>(rs2v);
        UWord       result  = UWord(0);

        switch (funct5) {
            case F5_AMOSWAP: result = rs2v;                                                    break;
            case F5_AMOADD:  result = loaded + rs2v;                                           break;
            case F5_AMOXOR:  result = loaded ^ rs2v;                                           break;
            case F5_AMOAND:  result = loaded & rs2v;                                           break;
            case F5_AMOOR:   result = loaded | rs2v;                                           break;
            case F5_AMOMIN:  result = static_cast<UWord>(sloaded < srs2v ? sloaded : srs2v);  break;
            case F5_AMOMAX:  result = static_cast<UWord>(sloaded > srs2v ? sloaded : srs2v);  break;
            case F5_AMOMINU: result = loaded < rs2v ? loaded : rs2v;                           break;
            case F5_AMOMAXU: result = loaded > rs2v ? loaded : rs2v;                           break;
            default:
                throw std::runtime_error("RVModel: unknown AMO funct5=0x" +
                                         std::to_string(funct5));
        }

        mem_.invalidateReservation();
        mem_.write(addr, static_cast<WordT>(result));
        regs_.set(d.rd, loaded);
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
                    result = ALU<XLEN>::execute(Op::MUL, rs1v, rs2v);
                    break;
                case F3_MULH:
                    result = ALU<XLEN>::execute(Op::MULH, rs1v, rs2v);
                    break;
                case F3_MULHSU:
                    result = ALU<XLEN>::execute(Op::MULHSU, rs1v, rs2v);
                    break;
                case F3_MULHU:
                    result = ALU<XLEN>::execute(Op::MULHU, rs1v, rs2v);
                    break;
                case F3_DIV:
                    result = ALU<XLEN>::execute(Op::DIV, rs1v, rs2v);
                    break;
                case F3_DIVU:
                    result = ALU<XLEN>::execute(Op::DIVU, rs1v, rs2v);
                    break;
                case F3_REM:
                    result = ALU<XLEN>::execute(Op::REM, rs1v, rs2v);
                    break;
                case F3_REMU:
                    result = ALU<XLEN>::execute(Op::REMU, rs1v, rs2v);
                    break;
                default:
                    throw std::runtime_error("RVModel: unknown M-ext funct3=0x" +
                                             std::to_string(d.funct3));
            }
        } else {
            switch (d.funct3) {
                case F3_ADD_SUB:
                    result =
                        ALU<XLEN>::execute((d.funct7 == F7_ALT) ? Op::SUB : Op::ADD, rs1v, rs2v);
                    break;
                case F3_SLL:
                    result = ALU<XLEN>::execute(Op::SLL, rs1v, rs2v);
                    break;
                case F3_SLT:
                    result = ALU<XLEN>::execute(Op::SLT, rs1v, rs2v);
                    break;
                case F3_SLTU:
                    result = ALU<XLEN>::execute(Op::SLTU, rs1v, rs2v);
                    break;
                case F3_XOR:
                    result = ALU<XLEN>::execute(Op::XOR, rs1v, rs2v);
                    break;
                case F3_SRL_SRA:
                    result =
                        ALU<XLEN>::execute((d.funct7 == F7_ALT) ? Op::SRA : Op::SRL, rs1v, rs2v);
                    break;
                case F3_OR:
                    result = ALU<XLEN>::execute(Op::OR, rs1v, rs2v);
                    break;
                case F3_AND:
                    result = ALU<XLEN>::execute(Op::AND, rs1v, rs2v);
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