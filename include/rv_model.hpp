#pragma once
#include <cassert>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "alu.hpp"
#include "config.hpp"
#include "csr_file.hpp"
#include "decoder.hpp"
#include "disasm.hpp"
#include "isa.hpp"
#include "memory_model.hpp"
#include "register_file.hpp"
#include "types.hpp"

// all 32 registers + pc
template <int XLEN = 32>
struct FullContext {
    typename XlenTraits<XLEN>::UWord pc       = 0;
    typename XlenTraits<XLEN>::UWord regs[32] = {};
};

// callee-saved registers + sp + ra + pc
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

enum class MemAccess { FETCH, LOAD, STORE };

template <int XLEN = 32, typename MemT = MemoryModel<XLEN>>
class RVModel {
public:
    using UWord = typename XlenTraits<XLEN>::UWord;
    using SWord = typename XlenTraits<XLEN>::SWord;
    using Addr  = typename XlenTraits<XLEN>::Addr;

    enum class PrivMode : uint32_t { U = 0, S = 1, M = 3 };

    RVModel(Config cfg, MemT& mem)
        : pc_(UWord(0)),
          config_(cfg),
          mem_(mem),
          halted_(false),
          instrCount_(0),
          debugMode_(false),
          priv_mode_(PrivMode::M) {}

    PrivMode privMode() const { return priv_mode_; }

    ~RVModel()                         = default;
    RVModel(const RVModel&)            = delete;
    RVModel& operator=(const RVModel&) = delete;

    RVModel(RVModel&& other) noexcept
        : pc_(other.pc_),
          config_(other.config_),
          mem_(other.mem_),
          regs_(std::move(other.regs_)),
          csr_(std::move(other.csr_)),
          halted_(other.halted_),
          instrCount_(other.instrCount_),
          debugMode_(other.debugMode_) {
        other.halted_ = true;
    }

    RVModel& operator=(RVModel&&) = delete;

    void init(Addr startPC = 0, UWord stackPointer = 0) {
        regs_       = RegisterFile<XLEN>();
        csr_        = CsrFile<XLEN>();
        pc_         = startPC;
        halted_     = false;
        instrCount_ = 0;
        priv_mode_  = PrivMode::M;
        if (stackPointer != UWord(0))
            regs_.set(2, stackPointer);  // x2 = sp
    }

    void step() {
        assert(!halted_ && "RVModel::step — called on halted CPU");
        assert((pc_ & UWord(0b11)) == UWord(0) && "RVModel::step — PC not 4-byte aligned");
        if (halted_)
            return;

        const auto paddr_fetch = translateAddr(Addr(pc_), MemAccess::FETCH);
        if (!paddr_fetch) {
            ++instrCount_;
            return;
        }

        Word raw;
        try {
            raw = mem_.readWord(*paddr_fetch);
        } catch (const std::out_of_range&) {
            fireTrap(UWord(CSR::EXC_INSN_FAULT), pc_);
            ++instrCount_;
            return;
        }

        if (raw == 0u) {
            if (debugMode_)
                std::cout << "[RVModel] HALT at PC=0x" << std::hex << pc_ << std::dec << "\n";
            halted_ = true;
            return;
        }

        const DecodedInstr<XLEN> instr = Decoder<XLEN>::decode(raw);
        const std::string        dis   = Disasm::disassemble(instr);
        if (debugMode_)
            std::cout << "[PC=0x" << std::hex << std::setw(8) << std::setfill('0') << pc_
                      << std::dec << "] " << dis << "\n";
        if (step_hook_)
            step_hook_(pc_, dis);

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
    CsrFile<XLEN>&      csr() { return csr_; }
    bool                isHalted() const { return halted_; }
    void                halt() { halted_ = true; }

    // fire interrupt if mstatus.MIE=1 and MIE register bit is set
    bool triggerInterrupt(UWord cause) {
        if (halted_)
            return false;
        const UWord mstatus = csr_.read(CSR::MSTATUS);
        if (!(mstatus & UWord(CSR::MSTATUS_MIE)))
            return false;
        const UWord code = cause & ~UWord(CSR::MCAUSE_INTERRUPT);
        if (!(csr_.read(CSR::MIE) & (UWord(1) << code)))
            return false;
        fireTrap(cause);
        return true;
    }
    uint64_t instrCount() const { return instrCount_; }
    void     setDebug(bool on) { debugMode_ = on; }

    // step hook: called before each instruction with (pc, disasm_string)
    using StepHook = std::function<void(UWord, const std::string&)>;
    void setStepHook(StepHook h) { step_hook_ = std::move(h); }

    // trap hook: called on every fireTrap with (cause, mepc, mtval)
    using TrapHook = std::function<void(UWord, UWord, UWord)>;
    void setTrapHook(TrapHook h) { trap_hook_ = std::move(h); }

    // save/restore callee-saved registers + sp + ra + pc
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
        regs_.set(1, ctx.ra);
        regs_.set(2, ctx.sp);
        regs_.set(8, ctx.s0);
        regs_.set(9, ctx.s1);
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
    void reset() {
        init(0, 0);
        debugMode_ = false;
    }

private:
    UWord              pc_;
    Config             config_;
    MemT&              mem_;
    RegisterFile<XLEN> regs_;
    CsrFile<XLEN>      csr_;
    bool               halted_;
    uint64_t           instrCount_;
    bool               debugMode_;
    PrivMode           priv_mode_;
    StepHook           step_hook_;
    TrapHook           trap_hook_;

    void advancePC() { pc_ += UWord(4); }

    static UWord pageFaultCause(MemAccess access) {
        switch (access) {
            case MemAccess::FETCH:
                return UWord(CSR::EXC_INSN_PAGE_FAULT);
            case MemAccess::LOAD:
                return UWord(CSR::EXC_LOAD_PAGE_FAULT);
            case MemAccess::STORE:
                return UWord(CSR::EXC_STORE_PAGE_FAULT);
        }
        return UWord(CSR::EXC_LOAD_PAGE_FAULT);
    }

    // Sv32 two-level page walk; returns physical address or fires trap and returns nullopt
    std::optional<Addr> translateAddr(Addr va, MemAccess access) {
        const UWord satp = csr_.read(CSR::SATP);
        if ((satp >> 31u) == 0u)
            return va;  // bare mode - no translation

        const UWord cause  = pageFaultCause(access);
        const UWord ppn    = satp & 0x3FFFFFu;
        const UWord vpn[2] = {(UWord(va) >> 12u) & 0x3FFu,   // vpn[0]: va[21:12]
                              (UWord(va) >> 22u) & 0x3FFu};  // vpn[1]: va[31:22]
        const UWord offset = UWord(va) & 0xFFFu;

        Addr a = static_cast<Addr>(ppn << 12u);

        for (int lvl = 1; lvl >= 0; --lvl) {
            const Addr pte_addr = static_cast<Addr>(a | (vpn[lvl] << 2u));
            UWord      pte;
            try {
                pte = static_cast<UWord>(mem_.readWord(pte_addr));
            } catch (...) {
                fireTrap(cause, UWord(va));
                return std::nullopt;
            }

            if (!(pte & 0x1u)) {  // V=0
                fireTrap(cause, UWord(va));
                return std::nullopt;
            }

            if (pte & 0xAu) {                               // R=1 or X=1 - leaf PTE
                if (lvl == 1 && ((pte >> 10u) & 0x3FFu)) {  // misaligned superpage
                    fireTrap(cause, UWord(va));
                    return std::nullopt;
                }
                // U-mode can only access pages with PTE.U=1 (bit 4)
                if (priv_mode_ == PrivMode::U && !(pte & (1u << 4u))) {
                    fireTrap(cause, UWord(va));
                    return std::nullopt;
                }
                // superpage: pa = pte.ppn[1]:va.vpn[0]:offset
                // regular:   pa = pte.ppn:offset
                const UWord pa = (lvl == 1)
                                     ? (((pte >> 20u) & 0xFFFu) << 22u) | (vpn[0] << 12u) | offset
                                     : ((pte >> 10u) << 12u) | offset;
                return static_cast<Addr>(pa);
            }

            if (lvl == 0) {  // non-leaf at bottom level
                fireTrap(cause, UWord(va));
                return std::nullopt;
            }
            a = static_cast<Addr>((pte >> 10u) << 12u);
        }
        return std::nullopt;  // unreachable
    }

    void fireTrap(UWord cause, UWord tval = UWord(0)) {
        if (trap_hook_)
            trap_hook_(cause, pc_, tval);
        UWord mtvec = csr_.getMTVEC();
        if (mtvec == UWord(0)) {
            halted_ = true;
            return;
        }
        csr_.setMEPC(pc_);
        csr_.setMCAUSE(cause);
        csr_.setMTVAL(tval);
        // mstatus: MPIE = MIE, MIE = 0, MPP = 11 (M-mode)
        UWord mstatus = csr_.read(CSR::MSTATUS);
        bool  mie     = (mstatus & UWord(CSR::MSTATUS_MIE)) != UWord(0);
        mstatus &= ~UWord(CSR::MSTATUS_MIE | CSR::MSTATUS_MPIE | CSR::MSTATUS_MPP);
        if (mie)
            mstatus |= UWord(CSR::MSTATUS_MPIE);
        mstatus |= static_cast<UWord>(priv_mode_) << 11u;  // MPP = current privilege mode
        csr_.write(CSR::MSTATUS, mstatus);
        priv_mode_ = PrivMode::M;
        pc_        = mtvec & ~UWord(3);  // direct mode: BASE & ~3
    }

    bool executeMRET() {
        // restore privilege from MPP, MIE = MPIE, MPIE = 1, MPP = U, pc = mepc
        UWord          mstatus = csr_.read(CSR::MSTATUS);
        bool           mpie    = (mstatus & UWord(CSR::MSTATUS_MPIE)) != UWord(0);
        const uint32_t mpp     = (static_cast<uint32_t>(mstatus) >> 11u) & 0x3u;
        priv_mode_             = static_cast<PrivMode>(mpp);
        mstatus &= ~UWord(CSR::MSTATUS_MIE | CSR::MSTATUS_MPIE | CSR::MSTATUS_MPP);
        if (mpie)
            mstatus |= UWord(CSR::MSTATUS_MIE);
        mstatus |= UWord(CSR::MSTATUS_MPIE);  // MPP already cleared to U (0)
        csr_.write(CSR::MSTATUS, mstatus);
        pc_ = csr_.getMEPC();
        return true;
    }

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
                if (target & UWord(0b11)) {
                    fireTrap(UWord(CSR::EXC_INSN_MISALIGN), target);
                    return true;
                }
                regs_.set(d.rd, ret);
                pc_ = target;
                return true;
            }

            case OP_JALR: {
                const UWord ret    = pc_ + UWord(4);
                const UWord target = (regs_.get(d.rs1) + static_cast<UWord>(d.imm)) & ~UWord(0b01);
                if (target & UWord(0b10)) {
                    fireTrap(UWord(CSR::EXC_INSN_MISALIGN), target);
                    return true;
                }
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
                if (d.funct3 == 0u) {
                    // ECALL: funct3=0, imm=0 - route through fireTrap based on privilege mode
                    if (d.imm == 0) {
                        const UWord cause = (priv_mode_ == PrivMode::U)   ? UWord(CSR::EXC_ECALL_U)
                                            : (priv_mode_ == PrivMode::S) ? UWord(CSR::EXC_ECALL_S)
                                                                          : UWord(CSR::EXC_ECALL_M);
                        fireTrap(cause);
                        return true;
                    } else if (d.imm == 0x302) {
                        // MRET: pc = mepc, restore mstatus.MIE from MPIE
                        return executeMRET();
                    } else if ((d.imm & 0xFE0) == 0x120) {
                        // SFENCE.VMA - NOP (no TLB in simulator)
                        return false;
                    } else {
                        // EBREAK or unknown → halt
                        if (debugMode_)
                            std::cout << "[RVModel] SYSTEM at PC=0x" << std::hex << pc_
                                      << " — HALTED\n";
                        halted_ = true;
                    }
                    return false;
                }
                return executeCSR(d);

            default:
                if (csr_.getMTVEC() != UWord(0)) {
                    fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                    return true;
                }
                throw std::runtime_error("RVModel: illegal opcode " + toHex(d.opcode) +
                                         " at PC=" + toHex(pc_));
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
                fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                return true;
        }

        if (taken) {
            const UWord target = pc_ + static_cast<UWord>(d.imm);
            if (target & UWord(0b11)) {
                fireTrap(UWord(CSR::EXC_INSN_MISALIGN), target);
                return true;
            }
            pc_ = target;
            return true;
        }
        return false;
    }

    bool executeLoad(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        const Addr addr = static_cast<Addr>(static_cast<SWord>(regs_.get(d.rs1)) + d.imm);

        // alignment: LW - 4-byte, LH/LHU - 2-byte, LB/LBU - no requirement
        if ((d.funct3 == F3_LW) && (UWord(addr) & UWord(0b11))) {
            fireTrap(UWord(CSR::EXC_LOAD_MISALIGN), UWord(addr));
            return true;
        }
        if ((d.funct3 == F3_LH || d.funct3 == F3_LHU) && (UWord(addr) & UWord(0b01))) {
            fireTrap(UWord(CSR::EXC_LOAD_MISALIGN), UWord(addr));
            return true;
        }

        const auto paddr_load = translateAddr(addr, MemAccess::LOAD);
        if (!paddr_load)
            return true;

        UWord result = UWord(0);
        try {
            switch (d.funct3) {
                case F3_LB:
                    result =
                        static_cast<UWord>(ISA::signExtend<XLEN>(mem_.readByte(*paddr_load), 8));
                    break;
                case F3_LH:
                    result =
                        static_cast<UWord>(ISA::signExtend<XLEN>(mem_.readHalf(*paddr_load), 16));
                    break;
                case F3_LW:
                    result = static_cast<UWord>(
                        static_cast<SWord>(static_cast<int32_t>(mem_.readWord(*paddr_load))));
                    break;
                case F3_LBU:
                    result = static_cast<UWord>(mem_.readByte(*paddr_load));
                    break;
                case F3_LHU:
                    result = static_cast<UWord>(mem_.readHalf(*paddr_load));
                    break;
                default:
                    fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                    return true;
            }
        } catch (const std::out_of_range&) {
            fireTrap(UWord(CSR::EXC_LOAD_FAULT), UWord(addr));
            return true;
        }

        regs_.set(d.rd, result);
        return false;
    }

    bool executeStore(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        const Addr  addr = static_cast<Addr>(static_cast<SWord>(regs_.get(d.rs1)) + d.imm);
        const UWord data = regs_.get(d.rs2);

        // alignment: SW - 4-byte, SH - 2-byte, SB - no requirement
        if ((d.funct3 == F3_SW) && (UWord(addr) & UWord(0b11))) {
            fireTrap(UWord(CSR::EXC_STORE_MISALIGN), UWord(addr));
            return true;
        }
        if ((d.funct3 == F3_SH) && (UWord(addr) & UWord(0b01))) {
            fireTrap(UWord(CSR::EXC_STORE_MISALIGN), UWord(addr));
            return true;
        }

        const auto paddr_store = translateAddr(addr, MemAccess::STORE);
        if (!paddr_store)
            return true;

        try {
            switch (d.funct3) {
                case F3_SB:
                    mem_.write(*paddr_store, static_cast<ByteT>(data & 0xFFu));
                    break;
                case F3_SH:
                    mem_.write(*paddr_store, static_cast<HalfT>(data & 0xFFFFu));
                    break;
                case F3_SW:
                    mem_.write(*paddr_store, static_cast<WordT>(data));
                    break;
                default:
                    fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                    return true;
            }
        } catch (const std::out_of_range&) {
            fireTrap(UWord(CSR::EXC_STORE_FAULT), UWord(addr));
            return true;
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
                result =
                    ALU<XLEN>::execute((d.funct7 & 0b00100000u) ? Op::SRA : Op::SRL, rs1v, immv);
                break;
            case F3_OR:
                result = ALU<XLEN>::execute(Op::OR, rs1v, immv);
                break;
            case F3_AND:
                result = ALU<XLEN>::execute(Op::AND, rs1v, immv);
                break;
            default:
                fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                return true;
        }

        regs_.set(d.rd, result);
        return false;
    }

    bool executeCSR(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        const uint16_t addr = static_cast<uint16_t>(d.imm & 0xFFF);
        // CSR addr[9:8] = minimum required privilege (0=U, 1=S, 3=M)
        if (static_cast<uint32_t>(priv_mode_) < ((addr >> 8u) & 0x3u)) {
            fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(addr));
            return true;
        }
        const UWord   rs1v = regs_.get(d.rs1);
        const uint8_t zimm = static_cast<uint8_t>(d.rs1);
        UWord         old  = UWord(0);

        switch (d.funct3) {
            case F3_CSRRW:
                old = csr_.csrrw(addr, rs1v);
                break;
            case F3_CSRRS:
                old = csr_.csrrs(addr, rs1v, d.rs1 == 0);
                break;
            case F3_CSRRC:
                old = csr_.csrrc(addr, rs1v, d.rs1 == 0);
                break;
            case F3_CSRRWI:
                old = csr_.csrrwi(addr, zimm);
                break;
            case F3_CSRRSI:
                old = csr_.csrrsi(addr, zimm);
                break;
            case F3_CSRRCI:
                old = csr_.csrrci(addr, zimm);
                break;
            default:
                fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                return true;
        }

        regs_.set(d.rd, old);
        return false;
    }

    // A-extension: LR/SC + AMO
    bool executeAMO(const DecodedInstr<XLEN>& d) {
        using namespace ISA;
        if (!config_.hasExtension(Config::EXT_A)) {
            fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
            return true;
        }

        const Addr    addr   = static_cast<Addr>(regs_.get(d.rs1));
        const UWord   rs2v   = regs_.get(d.rs2);
        const uint8_t funct5 = static_cast<uint8_t>(d.funct7 >> 2);

        // AMO - 4-byte aligned
        if (UWord(addr) & UWord(0b11)) {
            fireTrap(UWord(CSR::EXC_STORE_MISALIGN), UWord(addr));
            return true;
        }

        const auto paddr_amo = translateAddr(addr, MemAccess::STORE);
        if (!paddr_amo)
            return true;

        if (funct5 == F5_LR) {
            regs_.set(d.rd, static_cast<UWord>(mem_.readWord(*paddr_amo)));
            mem_.reserveLoad(*paddr_amo);
            return false;
        }

        if (funct5 == F5_SC) {
            const bool ok = mem_.storeConditional(*paddr_amo, static_cast<WordT>(rs2v));
            regs_.set(d.rd, ok ? UWord(0) : UWord(1));
            return false;
        }

        const UWord loaded  = static_cast<UWord>(mem_.readWord(*paddr_amo));
        const SWord sloaded = static_cast<SWord>(loaded);
        const SWord srs2v   = static_cast<SWord>(rs2v);
        UWord       result  = UWord(0);

        switch (funct5) {
            case F5_AMOSWAP:
                result = rs2v;
                break;
            case F5_AMOADD:
                result = loaded + rs2v;
                break;
            case F5_AMOXOR:
                result = loaded ^ rs2v;
                break;
            case F5_AMOAND:
                result = loaded & rs2v;
                break;
            case F5_AMOOR:
                result = loaded | rs2v;
                break;
            case F5_AMOMIN:
                result = static_cast<UWord>(sloaded < srs2v ? sloaded : srs2v);
                break;
            case F5_AMOMAX:
                result = static_cast<UWord>(sloaded > srs2v ? sloaded : srs2v);
                break;
            case F5_AMOMINU:
                result = loaded < rs2v ? loaded : rs2v;
                break;
            case F5_AMOMAXU:
                result = loaded > rs2v ? loaded : rs2v;
                break;
            default:
                fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                return true;
        }

        mem_.invalidateReservation();
        mem_.write(*paddr_amo, static_cast<WordT>(result));
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
            if (!config_.hasExtension(Config::EXT_M)) {
                fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                return true;
            }
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
                    fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                    return true;
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
                    fireTrap(UWord(CSR::EXC_ILLEGAL_INSN), UWord(d.opcode));
                    return true;
            }
        }

        regs_.set(d.rd, result);
        return false;
    }
};

// deduction guide: RVModel cpu(cfg, mem) where mem is MemoryModel<XLEN>&
template <int XLEN>
RVModel(Config, MemoryModel<XLEN>&) -> RVModel<XLEN, MemoryModel<XLEN>>;