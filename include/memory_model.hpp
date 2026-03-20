#pragma once
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "types.hpp"

template <int XLEN = 32>
class MemoryModel {
public:
    using Addr  = typename XlenTraits<XLEN>::Addr;
    using UWord = typename XlenTraits<XLEN>::UWord;

    static constexpr size_t DEFAULT_SIZE = 1u << 20;  // 1 MiB

    explicit MemoryModel(size_t size = DEFAULT_SIZE) : data_(size, 0u), size_(size) {
        assert(size > 0 && "MemoryModel: size must be > 0");
    }

    ~MemoryModel()                                 = default;
    MemoryModel(const MemoryModel&)                = default;
    MemoryModel& operator=(const MemoryModel&)     = default;
    MemoryModel(MemoryModel&&) noexcept            = default;
    MemoryModel& operator=(MemoryModel&&) noexcept = default;

    // lvalue overload (copies)
    void loadProgram(const std::vector<uint8_t>& program, Addr base = 0) {
        assert(!program.empty() && "loadProgram: program must not be empty");
        if (static_cast<size_t>(base) + program.size() > size_) {
            throw std::out_of_range("MemoryModel::loadProgram — program exceeds memory: base=0x" +
                                    std::to_string(base) +
                                    " size=" + std::to_string(program.size()) +
                                    " limit=" + std::to_string(size_));
        }
        std::copy(program.begin(), program.end(), data_.begin() + static_cast<ptrdiff_t>(base));
    }

    // rvalue overload (moves)
    void loadProgram(std::vector<uint8_t>&& program, Addr base = 0) {
        assert(!program.empty() && "loadProgram: program must not be empty");
        if (static_cast<size_t>(base) + program.size() > size_) {
            throw std::out_of_range("MemoryModel::loadProgram — program exceeds memory: base=0x" +
                                    std::to_string(base) +
                                    " size=" + std::to_string(program.size()) +
                                    " limit=" + std::to_string(size_));
        }
        std::move(program.begin(), program.end(), data_.begin() + static_cast<ptrdiff_t>(base));
    }

    ByteT readByte(Addr addr) const {
        checkBounds(addr, 1);
        return data_[addr];
    }

    HalfT readHalf(Addr addr) const {
        checkBounds(addr, 2);
        return static_cast<HalfT>(static_cast<unsigned>(data_[addr]) |
                                  (static_cast<unsigned>(data_[addr + 1]) << 8));
    }

    // always 32 bits: fetch, LW, SW
    WordT readWord(Addr addr) const {
        checkBounds(addr, 4);
        return static_cast<WordT>(data_[addr]) | (static_cast<WordT>(data_[addr + 1]) << 8) |
               (static_cast<WordT>(data_[addr + 2]) << 16) |
               (static_cast<WordT>(data_[addr + 3]) << 24);
    }

    void write(Addr addr, ByteT val) {
        checkBounds(addr, 1);
        data_[addr] = val;
    }

    void write(Addr addr, HalfT val) {
        checkBounds(addr, 2);
        data_[addr]     = static_cast<uint8_t>(val & 0xFFu);
        data_[addr + 1] = static_cast<uint8_t>((val >> 8) & 0xFFu);
    }

    void write(Addr addr, WordT val) {
        checkBounds(addr, 4);
        data_[addr]     = static_cast<uint8_t>(val & 0xFFu);
        data_[addr + 1] = static_cast<uint8_t>((val >> 8) & 0xFFu);
        data_[addr + 2] = static_cast<uint8_t>((val >> 16) & 0xFFu);
        data_[addr + 3] = static_cast<uint8_t>((val >> 24) & 0xFFu);
    }

    void dump(Addr from, size_t count) const {
        assert(count > 0 && "dump: count must be > 0");
        std::cout << "\n=== MEMORY DUMP [0x" << std::hex << from << " .. 0x" << (from + count)
                  << "] ===\n";
        for (size_t i = 0; i < count; i += 4) {
            Addr a = from + static_cast<Addr>(i);
            if (static_cast<size_t>(a) + 4 <= size_)
                std::cout << "0x" << std::setw(8) << std::setfill('0') << a << ": 0x"
                          << std::setw(8) << readWord(a) << "\n";
        }
        std::cout << std::dec;
    }

    size_t   size() const { return size_; }
    uint8_t* data() { return data_.data(); }

    // A-extension: LR/SC
    void reserveLoad(Addr addr) {
        lr_addr_  = addr;
        lr_valid_ = true;
    }

    bool storeConditional(Addr addr, WordT val) {
        if (!lr_valid_ || lr_addr_ != addr)
            return false;
        lr_valid_ = false;
        write(addr, val);
        return true;
    }

    void invalidateReservation() { lr_valid_ = false; }

private:
    std::vector<uint8_t> data_;
    size_t               size_;
    Addr                 lr_addr_  = Addr(0);
    bool                 lr_valid_ = false;

    void checkBounds(Addr addr, size_t width) const {
        assert((width == 1 || width == 2 || width == 4) && "checkBounds: width must be 1, 2, or 4");
        if (static_cast<size_t>(addr) + width > size_) {
            throw std::out_of_range("MemoryModel: OOB addr=0x" + std::to_string(addr) + " width=" +
                                    std::to_string(width) + " limit=" + std::to_string(size_));
        }
    }
};