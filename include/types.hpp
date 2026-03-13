#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>

// XLEN-parameterized type traits — one definition for RV32 and RV64
template <int XLEN>
struct XlenTraits {
    static_assert(XLEN == 32 || XLEN == 64, "XLEN must be 32 or 64");

    using UWord = std::conditional_t<XLEN == 32, uint32_t, uint64_t>;
    using SWord = std::conditional_t<XLEN == 32, int32_t, int64_t>;
    using Addr  = UWord;

    static constexpr int   SHIFT_MASK = XLEN - 1;  // 31 (RV32) or 63 (RV64)
    static constexpr UWord UWORD_MAX  = std::numeric_limits<UWord>::max();
    static constexpr SWord SWORD_MIN  = std::numeric_limits<SWord>::min();
};

// RV32 backward-compat aliases — existing code continues to compile unchanged
using Word        = typename XlenTraits<32>::UWord;  // uint32_t
using SWord       = typename XlenTraits<32>::SWord;  // int32_t
using DWord       = uint64_t;                        // double word (multiply intermediate)
using SDWord      = int64_t;                         // signed double word
using Addr        = typename XlenTraits<32>::Addr;   // uint32_t
using ByteT       = uint8_t;
using HalfT       = uint16_t;
using WordT       = uint32_t;
using DoubleWordT = uint64_t;

static_assert(sizeof(Word) == 4, "Word must be 4 bytes");
static_assert(sizeof(DWord) == 8, "DWord must be 8 bytes");
static_assert(sizeof(Addr) == 4, "Addr must be 4 bytes");