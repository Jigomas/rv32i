#include "../include/memory.hpp"

#include <iomanip>
#include <iostream>

Memory::Memory(size_t size) : data_(size, 0u), size_(size) {}

void Memory::loadProgram(const std::vector<uint8_t>& program, Addr base) {
    if (static_cast<size_t>(base) + program.size() > size_)
        throw std::runtime_error("Memory::loadProgram — program exceeds memory size");
    std::copy(program.begin(), program.end(), data_.begin() + base);
}

uint8_t Memory::loadByte(Addr addr) const {
    checkBounds(addr, 1);
    return data_[addr];
}

uint16_t Memory::loadHalf(Addr addr) const {
    checkBounds(addr, 2);
    return static_cast<uint16_t>(data_[addr]) | (static_cast<uint16_t>(data_[addr + 1]) << 8);
}

uint32_t Memory::loadWord(Addr addr) const {
    checkBounds(addr, 4);
    return static_cast<uint32_t>(data_[addr]) | (static_cast<uint32_t>(data_[addr + 1]) << 8) |
           (static_cast<uint32_t>(data_[addr + 2]) << 16) |
           (static_cast<uint32_t>(data_[addr + 3]) << 24);
}

void Memory::storeByte(Addr addr, uint8_t val) {
    checkBounds(addr, 1);
    data_[addr] = val;
}

void Memory::storeHalf(Addr addr, uint16_t val) {
    checkBounds(addr, 2);
    data_[addr]     = val & 0xFFu;
    data_[addr + 1] = (val >> 8) & 0xFFu;
}

void Memory::storeWord(Addr addr, uint32_t val) {
    checkBounds(addr, 4);
    data_[addr]     = val & 0xFFu;
    data_[addr + 1] = (val >> 8) & 0xFFu;
    data_[addr + 2] = (val >> 16) & 0xFFu;
    data_[addr + 3] = (val >> 24) & 0xFFu;
}

void Memory::dump(Addr from, size_t count) const {
    std::cout << "\n=== MEMORY DUMP [" << std::hex << "0x" << from << " .. 0x" << (from + count)
              << "] ===\n";
    for (size_t i = 0; i < count; i += 4) {
        Addr a = from + static_cast<Addr>(i);
        if (static_cast<size_t>(a) + 4 <= size_) {
            std::cout << "0x" << std::setw(8) << std::setfill('0') << a << ": 0x" << std::setw(8)
                      << loadWord(a) << "\n";
        }
    }
    std::cout << std::dec;
}

void Memory::checkBounds(Addr addr, size_t width) const {
    if (static_cast<size_t>(addr) + width > size_)
        throw std::runtime_error("Memory: out-of-bounds access at 0x" + std::to_string(addr));
}