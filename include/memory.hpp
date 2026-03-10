#pragma once
#include <cstdint>
#include <vector>

#include "types.hpp"

class Memory {
public:
    static constexpr size_t DEFAULT_SIZE = 1u << 20;  // 1 MiB

    explicit Memory(size_t size = DEFAULT_SIZE);

    void loadProgram(const std::vector<uint8_t>& program, Addr base = 0);

    uint8_t  loadByte(Addr addr) const;
    uint16_t loadHalf(Addr addr) const;
    uint32_t loadWord(Addr addr) const;

    void storeByte(Addr addr, uint8_t val);
    void storeHalf(Addr addr, uint16_t val);
    void storeWord(Addr addr, uint32_t val);

    void dump(Addr from, size_t count) const;

    size_t size() const { return size_; }

private:
    std::vector<uint8_t> data_;
    size_t               size_;

    void checkBounds(Addr addr, size_t width) const;
};