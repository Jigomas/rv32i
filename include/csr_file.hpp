#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "types.hpp"

namespace CSR {

constexpr uint16_t MVENDORID = 0xF11;  // vendor id
constexpr uint16_t MARCHID   = 0xF12;  // architecture id
constexpr uint16_t MIMPID    = 0xF13;  // implementation id
constexpr uint16_t MHARTID   = 0xF14;  // hart id (amount of cores = 1 => 0)

constexpr uint16_t SATP     = 0x180;  // supervisor address translation (Sv32)
constexpr uint16_t MSTATUS  = 0x300;  // machine status
constexpr uint16_t MISA     = 0x301;  // ISA and extensions
constexpr uint16_t MIE      = 0x304;  // machine interrupt enable
constexpr uint16_t MTVEC    = 0x305;  // trap-handler base address
constexpr uint16_t MSCRATCH = 0x340;  // trap scratch
constexpr uint16_t MEPC     = 0x341;  // exception PC
constexpr uint16_t MCAUSE   = 0x342;  // trap cause
constexpr uint16_t MTVAL    = 0x343;  // bad addr or instruction
constexpr uint16_t MIP      = 0x344;  // machine interrupt pending
constexpr uint16_t MCYCLE   = 0xB00;  // cycle counter
constexpr uint16_t MINSTRET = 0xB02;  // instructions retired

// mstatus
constexpr uint32_t MSTATUS_MIE  = (1u << 3);   // global machine interrupt enable
constexpr uint32_t MSTATUS_MPIE = (1u << 7);   // saved MIE before trap
constexpr uint32_t MSTATUS_MPP  = (3u << 11);  // previous privilege mode (2 bits)

// mtvec.mode
constexpr uint32_t MTVEC_MODE_DIRECT   = 0b0u;  // all traps to BASE
constexpr uint32_t MTVEC_MODE_VECTORED = 0b1u;  // async to BASE + 4*cause

// mcause: bit 31 = 1 - interrupt, 0 - exception
constexpr uint32_t MCAUSE_INTERRUPT = (1u << 31);

// exception codes
constexpr uint32_t EXC_INSN_MISALIGN    = 0b0000u;  // instruction address misaligned
constexpr uint32_t EXC_INSN_FAULT       = 0b0001u;  // instruction access fault
constexpr uint32_t EXC_ILLEGAL_INSN     = 0b0010u;  // illegal instruction
constexpr uint32_t EXC_BREAKPOINT       = 0b0011u;  // breakpoint
constexpr uint32_t EXC_LOAD_MISALIGN    = 0b0100u;  // load address misaligned
constexpr uint32_t EXC_LOAD_FAULT       = 0b0101u;  // load access fault
constexpr uint32_t EXC_STORE_MISALIGN   = 0b0110u;  // store address misaligned
constexpr uint32_t EXC_STORE_FAULT      = 0b0111u;  // store access fault
constexpr uint32_t EXC_ECALL_U          = 0b1000u;  // ecall from U-mode
constexpr uint32_t EXC_ECALL_M          = 0b1011u;  // ecall from M-mode
constexpr uint32_t EXC_INSN_PAGE_FAULT  = 0b1100u;  // instruction page fault
constexpr uint32_t EXC_LOAD_PAGE_FAULT  = 0b1101u;  // load page fault
constexpr uint32_t EXC_STORE_PAGE_FAULT = 0b1111u;  // store page fault

// interrupt codes
constexpr uint32_t INT_SW_M    = MCAUSE_INTERRUPT | 0b0011u;  // machine software interrupt
constexpr uint32_t INT_TIMER_M = MCAUSE_INTERRUPT | 0b0111u;  // machine timer interrupt
constexpr uint32_t INT_EXT_M   = MCAUSE_INTERRUPT | 0b1011u;  // machine external interrupt

}  // namespace CSR

template <int XLEN = 32>
class CsrFile {
public:
    using UWord = typename XlenTraits<XLEN>::UWord;

    static constexpr std::size_t NUM_CSRS = 4096;

    CsrFile() { regs_.fill(UWord(0)); }

    ~CsrFile()                             = default;
    CsrFile(const CsrFile&)                = default;
    CsrFile& operator=(const CsrFile&)     = default;
    CsrFile(CsrFile&&) noexcept            = default;
    CsrFile& operator=(CsrFile&&) noexcept = default;

    UWord read(uint16_t addr) const {
        assert(addr < NUM_CSRS && "CsrFile::read — address out of range");
        if (addr == CSR::MHARTID)
            return UWord(0);
        return regs_[addr];
    }

    void write(uint16_t addr, UWord val) {
        assert(addr < NUM_CSRS && "CsrFile::write — address out of range");
        if (isReadOnly(addr))
            return;
        regs_[addr] = val;
    }

    // CSRRW: t = csr; csr = rs1; rd = t
    UWord csrrw(uint16_t addr, UWord rs1val) {
        UWord old = read(addr);
        write(addr, rs1val);
        return old;
    }

    // CSRRS: t = csr; if (rs1 != x0) csr |= rs1; rd = t
    UWord csrrs(uint16_t addr, UWord rs1val, bool rs1IsZero) {
        UWord old = read(addr);
        if (!rs1IsZero)
            write(addr, old | rs1val);
        return old;
    }

    // CSRRC: t = csr; if (rs1 != x0) csr &= ~rs1; rd = t
    UWord csrrc(uint16_t addr, UWord rs1val, bool rs1IsZero) {
        UWord old = read(addr);
        if (!rs1IsZero)
            write(addr, old & ~rs1val);
        return old;
    }

    // CSRRWI: t = csr; csr = zimm; rd = t
    UWord csrrwi(uint16_t addr, uint8_t zimm) {
        UWord old = read(addr);
        write(addr, UWord(zimm));
        return old;
    }

    // CSRRSI: t = csr; if (zimm != 0) csr |= zimm; rd = t
    UWord csrrsi(uint16_t addr, uint8_t zimm) {
        UWord old = read(addr);
        if (zimm != 0)
            write(addr, old | UWord(zimm));
        return old;
    }

    // CSRRCI: t = csr; if (zimm != 0) csr &= ~zimm; rd = t
    UWord csrrci(uint16_t addr, uint8_t zimm) {
        UWord old = read(addr);
        if (zimm != 0)
            write(addr, old & ~UWord(zimm));
        return old;
    }

    bool  getMIE() const { return (regs_[CSR::MSTATUS] & UWord(CSR::MSTATUS_MIE)) != 0; }
    UWord getMTVEC() const { return regs_[CSR::MTVEC]; }
    UWord getMEPC() const { return regs_[CSR::MEPC]; }
    UWord getMCAUSE() const { return regs_[CSR::MCAUSE]; }
    UWord getMTVAL() const { return regs_[CSR::MTVAL]; }
    UWord getMSCRATCH() const { return regs_[CSR::MSCRATCH]; }

    void setMIE(bool enable) {
        if (enable)
            regs_[CSR::MSTATUS] |= UWord(CSR::MSTATUS_MIE);
        else
            regs_[CSR::MSTATUS] &= ~UWord(CSR::MSTATUS_MIE);
    }

    void setMEPC(UWord val) { regs_[CSR::MEPC] = val; }
    void setMCAUSE(UWord val) { regs_[CSR::MCAUSE] = val; }
    void setMTVAL(UWord val) { regs_[CSR::MTVAL] = val; }

    void dump() const {
        std::cout << "\n=== CSR FILE (XLEN=" << XLEN << ") ===\n";
        const struct {
            const char* name;
            uint16_t    addr;
        } entries[] = {
            {"mstatus ", CSR::MSTATUS},
            {"mie     ", CSR::MIE},
            {"mtvec   ", CSR::MTVEC},
            {"mscratch", CSR::MSCRATCH},
            {"mepc    ", CSR::MEPC},
            {"mcause  ", CSR::MCAUSE},
            {"mtval   ", CSR::MTVAL},
            {"mip     ", CSR::MIP},
        };
        for (const auto& e : entries) {
            std::cout << e.name << ": 0x" << std::hex << std::setw(XLEN / 4) << std::setfill('0')
                      << read(e.addr) << std::dec << "\n";
        }
    }

private:
    std::array<UWord, NUM_CSRS> regs_;

    // csr[11:10] == 11 means read-only (RISC-V Privileged Spec)
    static bool isReadOnly(uint16_t addr) { return (addr >> 10u) == 0b11u; }
};
