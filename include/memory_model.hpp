#pragma once
#include <cstdint>
#include <vector>

#include "types.hpp"

class MemoryModel {
public:
    static constexpr size_t DEFAULT_SIZE = 1u << 20;  // 1 MiB

    explicit MemoryModel(size_t size = DEFAULT_SIZE);
    ~MemoryModel()                                 = default;
    MemoryModel(const MemoryModel&)                = default;
    MemoryModel& operator=(const MemoryModel&)     = default;
    MemoryModel(MemoryModel&&) noexcept            = default;
    MemoryModel& operator=(MemoryModel&&) noexcept = default;

    void loadProgram(const std::vector<uint8_t>& program, Addr base = 0);

    ByteT readByte(Addr addr) const;
    HalfT readHalf(Addr addr) const;
    WordT readWord(Addr addr) const;

    void write(Addr addr, ByteT val);
    void write(Addr addr, HalfT val);
    void write(Addr addr, WordT val);

    void   dump(Addr from, size_t count) const;
    size_t size() const { return size_; }

private:
    std::vector<uint8_t> data_;
    size_t               size_;

    void checkBounds(Addr addr, size_t width) const;
};