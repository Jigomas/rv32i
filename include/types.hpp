#pragma once
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>

// XLEN type traits: UWord/SWord/Addr for RV32 and RV64
template <int XLEN>
struct XlenTraits {
    static_assert(XLEN == 32 || XLEN == 64, "XLEN must be 32 or 64");

    using UWord = std::conditional_t<XLEN == 32, uint32_t, uint64_t>;
    using SWord = std::conditional_t<XLEN == 32, int32_t, int64_t>;
    using Addr  = UWord;

    static constexpr int   SHIFT_MASK = XLEN - 1;  // 31 (RV32) or 63 (RV64)
    static constexpr UWord UWORD_MAX  = std::numeric_limits<UWord>::max();  // 4294967295
    static constexpr UWord UWORD_MIN  = std::numeric_limits<UWord>::min();  // 0
    static constexpr SWord SWORD_MIN  = std::numeric_limits<SWord>::min();  // -2147483648
    static constexpr SWord SWORD_MAX  = std::numeric_limits<SWord>::max();  // 2147483647
};

using Word        = typename XlenTraits<32>::UWord;  // register value
using SWord       = typename XlenTraits<32>::SWord;  // signed register value
using DWord       = uint64_t;                        // multiply intermediate (32x32=64)
using SDWord      = int64_t;                         // signed multiply intermediate
using Addr        = typename XlenTraits<32>::Addr;   // memory address
using ByteT       = uint8_t;                         // LB/SB
using HalfT       = uint16_t;                        // LH/SH
using WordT       = uint32_t;                        // LW/SW
using DoubleWordT = uint64_t;                        // LD/SD (RV64)
static_assert(sizeof(Word) == 4, "Word must be 4 bytes");
static_assert(sizeof(DWord) == 8, "DWord must be 8 bytes");
static_assert(sizeof(Addr) == 4, "Addr must be 4 bytes");

inline std::string toHex(uint64_t v) {
    std::ostringstream ss;
    ss << "0x" << std::hex << v;
    return ss.str();
}