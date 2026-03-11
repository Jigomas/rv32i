#pragma once
#include "config.hpp"
#include "decoder.hpp"
#include "memory.hpp"
#include "registers.hpp"
#include "types.hpp"

class CPU {
public:
    CPU(Config cfg, Memory& mem);

    ~CPU()                     = default;
    CPU(const CPU&)            = delete;
    CPU& operator=(const CPU&) = delete;
    CPU(CPU&& other) noexcept;
    CPU& operator=(CPU&&) = delete;

    void setPC(Word addr);
    void execute();

    void run(uint64_t maxSteps = 0);

    Register& regs();
    bool      isHalted() const;
    uint64_t  instrCount() const;

    void setDebug(bool on);
    void setStackPointer(Word sp);
    void reset();

private:
    Config   config_;
    Memory&  mem_;
    Register regs_;
    bool     halted_;
    uint64_t instrCount_;
    bool     debugMode_;

    bool executeInstr(const DecodedInstr& d);
    bool executeBranch(const DecodedInstr& d);
    bool executeLoad(const DecodedInstr& d);
    bool executeStore(const DecodedInstr& d);
    bool executeOpImm(const DecodedInstr& d);
    bool executeOp(const DecodedInstr& d);
};