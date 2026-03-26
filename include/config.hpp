#pragma once
#include <cassert>
#include <cstdint>

struct Config {
    enum Extension : uint32_t {
        EXT_NONE = 0b00000,
        EXT_M    = 0b00001,  // Integer multiply/divide
        EXT_A    = 0b00010,  // Atomic ops
        EXT_F    = 0b00100,  // Float 32-bit       [not implemented]
        EXT_D    = 0b01000,  // Double 64-bit      [not implemented]
        EXT_C    = 0b10000,  // Compressed 16-bit  [not implemented]
    };

    uint32_t extensions;

    explicit Config(uint32_t ext = EXT_NONE) : extensions(ext) { validate(); }

    bool hasExtension(Extension e) const { return (extensions & static_cast<uint32_t>(e)) != 0; }

    void validate() const {
#ifndef NDEBUG
        constexpr uint32_t IMPLEMENTED = EXT_M | EXT_A;
        assert(!(extensions & ~IMPLEMENTED) &&
               "Config: extension not implemented (F/D/C are stubs)");
#endif
    }
};