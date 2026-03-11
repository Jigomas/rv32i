#pragma once
#include <cstdint>

#include "config.hpp"
#include "decoder.hpp"
#include "memory_model.hpp"
#include "register_file.hpp"
#include "types.hpp"

class RVModel {
public:
    RVModel(Config cfg, MemoryModel& mem);

    ~RVModel()                         = default;
    RVModel(const RVModel&)            = delete;
    RVModel& operator=(const RVModel&) = delete;
    RVModel(RVModel&& other) noexcept;
    RVModel& operator=(RVModel&&)      = delete;

    void init(Addr startPC = 0, Word stackPointer = 0);
    void execute();

    Word getPC() const { return pc_; }
    void setPC(Addr addr);

    void run(uint64_t maxSteps = 0);

    RegisterFile& regs();
    bool          isHalted() const;
    uint64_t      instrCount() const;

    void setDebug(bool on);
    void reset();

private:
    Word         pc_;
    Config       config_;
    MemoryModel& mem_;
    RegisterFile regs_;
    bool         halted_;
    uint64_t     instrCount_;
    bool         debugMode_;

    void advancePC() { pc_ += 4u; }

    bool executeInstr(const DecodedInstr& d);
    bool executeBranch(const DecodedInstr& d);
    bool executeLoad(const DecodedInstr& d);
    bool executeStore(const DecodedInstr& d);
    bool executeOpImm(const DecodedInstr& d);
    bool executeOp(const DecodedInstr& d);
};