#pragma once
#include <cstdint>
#include <stdexcept>

struct Config {
    static constexpr int XLEN = 32;

    enum Extension : uint32_t {
        EXT_NONE = 0,
        EXT_M    = (1u << 0),  // Integer multiply/divide
        EXT_A    = (1u << 1),  // Atomic ops
        EXT_F    = (1u << 2),  // Float 32-bit       [stub]
        EXT_D    = (1u << 3),  // Double 64-bit      [stub]
        EXT_C    = (1u << 4),  // Compressed 16-bit  [stub]
    };

    uint32_t extensions;

    explicit Config(uint32_t ext = EXT_NONE) : extensions(ext) { validate(); }

    bool hasExtension(Extension e) const { return (extensions & static_cast<uint32_t>(e)) != 0; }

    void validate() const {
        constexpr uint32_t IMPLEMENTED = EXT_NONE | EXT_M | EXT_A;
        if (extensions & ~IMPLEMENTED) {
            throw std::invalid_argument("Config: extension not implemented (F/D/C are stubs)");
        }
    }
};