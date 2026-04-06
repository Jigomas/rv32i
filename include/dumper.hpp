#pragma once
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "csr_file.hpp"
#include "memory_model.hpp"
#include "register_file.hpp"

// human-readable name for mcause value
inline const char* causeName(uint32_t cause) {
    const bool     is_int = (cause & CSR::MCAUSE_INTERRUPT) != 0u;
    const uint32_t code   = cause & ~CSR::MCAUSE_INTERRUPT;
    if (is_int) {
        switch (code) {
            case 3u:
                return "INT_SW_M";
            case 7u:
                return "INT_TIMER_M";
            case 11u:
                return "INT_EXT_M";
            default:
                return "INT_?";
        }
    }
    switch (code) {
        case CSR::EXC_INSN_MISALIGN:
            return "EXC_INSN_MISALIGN";
        case CSR::EXC_INSN_FAULT:
            return "EXC_INSN_FAULT";
        case CSR::EXC_ILLEGAL_INSN:
            return "EXC_ILLEGAL_INSN";
        case CSR::EXC_BREAKPOINT:
            return "EXC_BREAKPOINT";
        case CSR::EXC_LOAD_MISALIGN:
            return "EXC_LOAD_MISALIGN";
        case CSR::EXC_LOAD_FAULT:
            return "EXC_LOAD_FAULT";
        case CSR::EXC_STORE_MISALIGN:
            return "EXC_STORE_MISALIGN";
        case CSR::EXC_STORE_FAULT:
            return "EXC_STORE_FAULT";
        case CSR::EXC_ECALL_U:
            return "EXC_ECALL_U";
        case CSR::EXC_ECALL_S:
            return "EXC_ECALL_S";
        case CSR::EXC_ECALL_M:
            return "EXC_ECALL_M";
        case CSR::EXC_INSN_PAGE_FAULT:
            return "EXC_INSN_PAGE_FAULT";
        case CSR::EXC_LOAD_PAGE_FAULT:
            return "EXC_LOAD_PAGE_FAULT";
        case CSR::EXC_STORE_PAGE_FAULT:
            return "EXC_STORE_PAGE_FAULT";
        default:
            return "EXC_?";
    }
}

// Dumper<XLEN>: execution trace log (ETL) + on-demand state/memory dumps.
//
// Wire into RVModel via setTrapHook / setStepHook:
//
//   Dumper<32> dbg("trace.txt");
//   cpu.setStepHook([&](uint32_t pc, const std::string& dis) { dbg.onStep(pc, dis); });
//   cpu.setTrapHook([&](uint32_t c, uint32_t mepc, uint32_t tval) {
//       dbg.onTrap(c, mepc, tval, mtime);
//   });
//   // on exit:
//   dbg.dumpState(cpu.regs(), cpu.csr(), cpu.getPC());
//   dbg.dumpClint(mtime, mtimecmp);
//   dbg.dumpMemHex(mem, 0x3000, 256);
template <int XLEN = 32>
class Dumper {
public:
    using UWord = typename XlenTraits<XLEN>::UWord;

    // trace_path: path for execution trace file (.txt); empty = no trace
    // trap_out: stream for trap events and state dumps (default: stderr)
    explicit Dumper(const std::string& trace_path = "", std::ostream& trap_out = std::cerr)
        : trap_out_(trap_out) {
        if (!trace_path.empty()) {
            trace_file_.open(trace_path);
            if (!trace_file_)
                throw std::runtime_error("Dumper: cannot open trace file: " + trace_path);
        }
    }

    ~Dumper() = default;

    // called for every instruction before execution; writes to ETL file if open
    void onStep(UWord pc, const std::string& disasm) {
        if (!trace_file_.is_open())
            return;
        trace_file_ << "[" << std::hex << std::setw(8) << std::setfill('0') << pc << "] " << disasm
                    << "\n";
    }

    // called on every fireTrap; mtime is passed from the simulator step loop
    void onTrap(UWord cause, UWord mepc, UWord mtval, uint64_t mtime = 0) {
        trap_out_ << "!TRAP" << "  cause=0x" << std::hex << std::setw(8) << std::setfill('0')
                  << cause << " (" << causeName(cause) << ")" << "  mepc=0x" << std::setw(8) << mepc
                  << "  mtval=0x" << std::setw(8) << mtval << "  mtime=" << std::dec << mtime
                  << "\n"
                  << std::flush;
    }

    // also write trap to ETL file so it appears inline in the trace
    void onTrapEtl(UWord cause, UWord mepc, UWord mtval, uint64_t mtime = 0) {
        if (!trace_file_.is_open())
            return;
        trace_file_ << "!TRAP" << "  cause=0x" << std::hex << std::setw(8) << std::setfill('0')
                    << cause << " (" << causeName(cause) << ")" << "  mepc=0x" << std::setw(8)
                    << mepc << "  mtval=0x" << std::setw(8) << mtval << "  mtime=" << std::dec
                    << mtime << "\n";
    }

    void dumpState(const RegisterFile<XLEN>& regs, const CsrFile<XLEN>& csr, UWord pc) {
        auto& o = trap_out_;
        o << "\n=== CPU STATE ===\n";
        o << "PC = 0x" << std::hex << std::setw(8) << std::setfill('0') << pc << std::dec << "\n";

        // registers: 4 per row, aligned columns
        for (unsigned i = 0; i < 32u; i += 4u) {
            for (unsigned j = 0; j < 4u; ++j) {
                const unsigned idx = i + j;
                o << "x" << std::setw(2) << std::setfill('0') << std::dec << idx << "("
                  << std::setw(4) << std::setfill(' ') << RegisterFile<XLEN>::ABI_NAMES[idx]
                  << ")=0x" << std::hex << std::setw(8) << std::setfill('0') << regs.get(idx)
                  << "  ";
            }
            o << "\n";
        }

        // key CSRs
        o << "\n--- CSRs ---\n";
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
            {"satp    ", CSR::SATP},
        };
        for (const auto& e : entries) {
            const UWord val = csr.read(e.addr);
            o << e.name << " = 0x" << std::hex << std::setw(8) << std::setfill('0') << val;
            // annotate mcause
            if (e.addr == CSR::MCAUSE)
                o << "  (" << causeName(val) << ")";
            o << std::dec << "\n";
        }
        o << std::flush;
    }

    void dumpClint(uint64_t mtime, uint64_t mtimecmp) {
        trap_out_ << "\n--- CLINT ---\n"
                  << "mtime    = " << mtime << "\n"
                  << "mtimecmp = " << mtimecmp << "\n"
                  << std::flush;
    }

    // classic hex+ASCII dump; size rounded up to 16-byte rows
    void dumpMemHex(const MemoryModel<XLEN>& mem, uint32_t from, uint32_t size) {
        auto& o = trap_out_;
        o << "\n--- MEM 0x" << std::hex << std::setw(8) << std::setfill('0') << from << " .. 0x"
          << std::setw(8) << (from + size) << " ---\n";

        for (uint32_t row = 0; row < size; row += 16u) {
            o << "0x" << std::hex << std::setw(8) << std::setfill('0') << (from + row) << ":  ";
            // hex bytes
            for (uint32_t col = 0; col < 16u; ++col) {
                if (col == 8u)
                    o << " ";
                const uint32_t addr = from + row + col;
                if (row + col < size && addr < static_cast<uint32_t>(mem.size())) {
                    uint8_t b = 0;
                    try {
                        b = mem.readByte(static_cast<typename XlenTraits<XLEN>::Addr>(addr));
                    } catch (...) {
                        b = 0xFFu;
                    }
                    o << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b) << " ";
                } else {
                    o << "   ";
                }
            }
            o << " |";
            // ASCII
            for (uint32_t col = 0; col < 16u; ++col) {
                const uint32_t addr = from + row + col;
                if (row + col < size && addr < static_cast<uint32_t>(mem.size())) {
                    uint8_t b = 0;
                    try {
                        b = mem.readByte(static_cast<typename XlenTraits<XLEN>::Addr>(addr));
                    } catch (...) {
                        b = 0u;
                    }
                    o << static_cast<char>(std::isprint(b) ? b : '.');
                } else {
                    o << " ";
                }
            }
            o << "|\n";
        }
        o << std::dec << std::flush;
    }

private:
    std::ofstream trace_file_;
    std::ostream& trap_out_;
};
