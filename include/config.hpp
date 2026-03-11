#pragma once
#include <cstdint>

struct Config {
    static constexpr int XLEN = 32;

    enum Extension : uint32_t { EXT_NONE = 0, EXT_M = (1u << 0) };

    uint32_t extensions;  // extensions mask

    explicit Config(uint32_t ext = EXT_NONE) : extensions(ext) {}

    bool hasExtension(Extension e) const { return (extensions & static_cast<uint32_t>(e)) != 0; }
};