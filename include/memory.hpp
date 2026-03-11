#pragma once
#include <cstdint>
#include <vector>

#include "types.hpp"

class Memory {
public:
    static constexpr size_t DEFAULT_SIZE = 1u << 20;  // 1 MiB
    explicit Memory(size_t size = DEFAULT_SIZE);
    ~Memory() = default;

    Memory(const Memory& other)                = default;
    Memory& operator=(const Memory& other)     = default;
    Memory(Memory&& other) noexcept            = default;
    Memory& operator=(Memory&& other) noexcept = default;

    void loadProgram(const std::vector<uint8_t>& program, Addr base = 0);

    ByteT readByte(Addr addr) const;
    HalfT readHalf(Addr addr) const;
    WordT readWord(Addr addr) const;

    uint8_t  loadByte(Addr addr) const { return readByte(addr); }
    uint16_t loadHalf(Addr addr) const { return readHalf(addr); }
    uint32_t loadWord(Addr addr) const { return readWord(addr); }

    void write(Addr addr, ByteT val);
    void write(Addr addr, HalfT val);
    void write(Addr addr, WordT val);

    void storeByte(Addr addr, uint8_t val) { write(addr, static_cast<ByteT>(val)); }
    void storeHalf(Addr addr, uint16_t val) { write(addr, static_cast<HalfT>(val)); }
    void storeWord(Addr addr, uint32_t val) { write(addr, static_cast<WordT>(val)); }

    void   dump(Addr from, size_t count) const;
    size_t size() const { return size_; }

private:
    std::vector<uint8_t> data_;
    size_t               size_;

    void checkBounds(Addr addr, size_t width) const;
};