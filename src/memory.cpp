#include "../include/memory.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>

Memory::Memory(size_t size)
    : data_(size, 0u)
    , size_(size) {
    assert(size > 0 && "Memory size must be greater than zero");
}

void Memory::loadProgram(const std::vector<uint8_t>& program, Addr base) {
    assert(!program.empty() && "loadProgram: program must not be empty");

    if (static_cast<size_t>(base) + program.size() > size_) {
        throw std::out_of_range(
            "Memory::loadProgram — program exceeds memory size: base=0x"
            + std::to_string(base)
            + " program_size=" + std::to_string(program.size())
            + " memory_size=" + std::to_string(size_));
    }
    std::copy(program.begin(), program.end(), data_.begin() + base);
}

ByteT Memory::readByte(Addr addr) const {
    checkBounds(addr, 1);
    return data_[addr];
}

HalfT Memory::readHalf(Addr addr) const {
    checkBounds(addr, 2);
    // little-endian: lower address = lower byte
    return static_cast<HalfT>(data_[addr]) | (static_cast<HalfT>(data_[addr + 1]) << 8);
}

WordT Memory::readWord(Addr addr) const {
    checkBounds(addr, 4);
    return static_cast<WordT>(data_[addr])
         | (static_cast<WordT>(data_[addr + 1]) << 8)
         | (static_cast<WordT>(data_[addr + 2]) << 16)
         | (static_cast<WordT>(data_[addr + 3]) << 24);
}

void Memory::write(Addr addr, ByteT val) {
    checkBounds(addr, 1);
    data_[addr] = val;
}

void Memory::write(Addr addr, HalfT val) {
    checkBounds(addr, 2);
    data_[addr]     = val & 0xFFu;
    data_[addr + 1] = (val >> 8) & 0xFFu;
}

void Memory::write(Addr addr, WordT val) {
    checkBounds(addr, 4);
    data_[addr]     = val & 0xFFu;
    data_[addr + 1] = (val >> 8) & 0xFFu;
    data_[addr + 2] = (val >> 16) & 0xFFu;
    data_[addr + 3] = (val >> 24) & 0xFFu;
}

void Memory::dump(Addr from, size_t count) const {
    assert(count > 0 && "dump: count must be > 0");

    std::cout << "\n=== MEMORY DUMP [0x" << std::hex << from
              << " .. 0x" << (from + count) << "] ===\n";
    for (size_t i = 0; i < count; i += 4) {
        Addr a = from + static_cast<Addr>(i);
        if (static_cast<size_t>(a) + 4 <= size_) {
            std::cout << "0x" << std::setw(8) << std::setfill('0') << a
                      << ": 0x" << std::setw(8) << readWord(a) << "\n";
        }
    }
    std::cout << std::dec;
}

void Memory::checkBounds(Addr addr, size_t width) const {
    assert((width == 1 || width == 2 || width == 4) && "checkBounds: width must be 1, 2, or 4");

    if (static_cast<size_t>(addr) + width > size_) {
        throw std::out_of_range(
            "Memory: out-of-bounds access at addr=0x" + std::to_string(addr)
            + " width=" + std::to_string(width)
            + " memory_size=" + std::to_string(size_));
    }
}