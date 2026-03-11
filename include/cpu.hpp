#pragma once
#include "config.hpp"
#include "decoder.hpp"
#include "memory.hpp"
#include "registers.hpp"
#include "types.hpp"

class CPU {
public:
    CPU(Config cfg, Memory& mem);

    void run(uint64_t maxSteps = 0);

    void step();

    bool execute(const DecodedInstr& d);

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

    bool executeBranch(const DecodedInstr& d);
    bool executeLoad(const DecodedInstr& d);
    bool executeStore(const DecodedInstr& d);
    bool executeOpImm(const DecodedInstr& d);
    bool executeOp(const DecodedInstr& d);
};