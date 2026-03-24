#pragma once
#include <list>
#include <unordered_map>

#include "../../include/memory_model.hpp"
#include "../../include/types.hpp"

// LRU cache model that wraps a MemoryModel and intercepts all reads/writes.
// Granularity: 4-byte words.  Policy: write-through, read-allocate.
//
// Based on Jigomas/LFU_cache — algorithm replaced with LRU.
// Evicts the least-recently-used word when the cache is full.
// Tracks hits and misses for performance analysis.

template <int XLEN = 32>
class CacheModel {
public:
    using UWord = typename XlenTraits<XLEN>::UWord;
    using Addr  = typename XlenTraits<XLEN>::Addr;

    explicit CacheModel(MemoryModel<XLEN>& backing, size_t capacity = 64)
        : backing_(backing), capacity_(capacity) {}

    ~CacheModel()                            = default;
    CacheModel(const CacheModel&)            = delete;
    CacheModel& operator=(const CacheModel&) = delete;
    CacheModel(CacheModel&&) noexcept        = delete;
    CacheModel& operator=(CacheModel&&)      = delete;

    ByteT readByte(Addr addr) {
        const WordT w = fetchWord(addr & ~Addr(0b11u));
        return static_cast<ByteT>((w >> ((addr & 0b11u) * 8u)) & 0xFFu);
    }

    HalfT readHalf(Addr addr) {
        const WordT w = fetchWord(addr & ~Addr(0b11u));
        return static_cast<HalfT>((w >> ((addr & 0b11u) * 8u)) & 0xFFFFu);
    }

    WordT readWord(Addr addr) { return fetchWord(addr); }

    // write-through: always write to backing; update cached line if present
    void write(Addr addr, ByteT val) {
        backing_.write(addr, val);
        patchCached(addr & ~Addr(0b11u), addr & 0b11u, 1, WordT(val));
    }
    void write(Addr addr, HalfT val) {
        backing_.write(addr, val);
        patchCached(addr & ~Addr(0b11u), addr & 0b11u, 2, WordT(val));
    }
    void write(Addr addr, WordT val) {
        backing_.write(addr, val);
        auto it = index_.find(addr);
        if (it != index_.end()) {
            it->second->value = val;
            lru_.splice(lru_.begin(), lru_, it->second);
        }
    }

    // pass binary loading through to backing (no cache warming)
    void loadProgram(const std::vector<uint8_t>& p, Addr base = 0) {
        backing_.loadProgram(p, base);
    }
    void loadProgram(std::vector<uint8_t>&& p, Addr base = 0) {
        backing_.loadProgram(std::move(p), base);
    }

    // A-extension: forward LR/SC reservation to backing
    void reserveLoad(Addr addr) { backing_.reserveLoad(addr); }
    bool storeConditional(Addr addr, WordT val) {
        const bool ok = backing_.storeConditional(addr, val);
        if (ok) {
            // keep cache coherent - update cached line with new value
            auto it = index_.find(addr & ~Addr(0b11u));
            if (it != index_.end())
                it->second->value = val;
        }
        return ok;
    }
    void invalidateReservation() { backing_.invalidateReservation(); }

    // expose backing so binary can be loaded directly before wrapping
    uint8_t* data() { return backing_.data(); }

    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    uint64_t accesses() const { return hits_ + misses_; }
    double   hitRate() const {
        const uint64_t total = hits_ + misses_;
        return total > 0 ? static_cast<double>(hits_) / static_cast<double>(total) : 0.0;
    }
    size_t size() const { return index_.size(); }
    size_t capacity() const { return capacity_; }

private:
    struct Line {
        Addr  tag;
        WordT value;
    };

    WordT fetchWord(Addr word_addr) {
        auto it = index_.find(word_addr);
        if (it != index_.end()) {
            lru_.splice(lru_.begin(), lru_, it->second);
            ++hits_;
            return it->second->value;
        }
        ++misses_;
        if (index_.size() >= capacity_) {
            index_.erase(lru_.back().tag);
            lru_.pop_back();
        }
        const WordT val = backing_.readWord(word_addr);
        lru_.push_front({word_addr, val});
        index_[word_addr] = lru_.begin();
        return val;
    }

    void patchCached(Addr word_addr, Addr byte_off, unsigned width, WordT val) {
        auto it = index_.find(word_addr);
        if (it == index_.end())
            return;
        const WordT    mask  = (width == 1) ? WordT(0xFFu) : WordT(0xFFFFu);
        const uint32_t shift = static_cast<uint32_t>(byte_off) * 8u;
        it->second->value    = (it->second->value & ~(mask << shift)) | ((val & mask) << shift);
        lru_.splice(lru_.begin(), lru_, it->second);
    }

    MemoryModel<XLEN>& backing_;
    size_t             capacity_;
    uint64_t           hits_   = 0;
    uint64_t           misses_ = 0;

    std::list<Line>                                              lru_;
    std::unordered_map<Addr, typename std::list<Line>::iterator> index_;
};
