#pragma once

#include <algorithm>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <limits>
#include <optional>



template<typename KeyT, typename ValueT>
class IdealCache {
public:
    explicit IdealCache(size_t capacity = 0);
    
    void                    LoadAccessPattern(KeyT key, const std::vector<size_t>& access_positions);
    std::optional<ValueT>   Get(KeyT key);
    void                    Put(const KeyT& key, const ValueT& value, size_t current_position);
    bool                    Contains(KeyT key) const;
    void                    DumpCache() const;
    
    size_t GetCurrentSize() const;
    size_t GetMaxSize() const;

private:
    void Remove();
    void UpdateNextUses();

    struct CacheEntry {
        ValueT      data;
        size_t      next_use;
        KeyT        key;
        
        bool operator<(const CacheEntry& other) const {
            return next_use > other.next_use;
        }
    };

    struct KeyAccess {
        std::vector<size_t> accesses;
        size_t              current_index;
    };

    std::unordered_map<KeyT, KeyAccess>    access_sequence_;
    std::unordered_map<KeyT, CacheEntry>   data_;
    size_t                                 capacity_;
    size_t                                 size_;
    size_t                                 current_access_index_;
};



template<typename KeyT, typename ValueT>
IdealCache<KeyT, ValueT>::IdealCache(size_t capacity)
    : capacity_(capacity),
      size_(0),
      current_access_index_(0)
{
    data_.reserve(capacity_);
    access_sequence_.reserve(capacity_ * 3);
}



template<typename KeyT, typename ValueT>
void IdealCache<KeyT, ValueT>::LoadAccessPattern(KeyT key, const std::vector<size_t>& access_positions) {
    access_sequence_[key] = KeyAccess{access_positions, 0};
}



template<typename KeyT, typename ValueT>
std::optional<ValueT> IdealCache<KeyT, ValueT>::Get(KeyT key) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }
    
    ++current_access_index_;
    UpdateNextUses();
    
    return it->second.data;
}



template<typename KeyT, typename ValueT>
void IdealCache<KeyT, ValueT>::Put(const KeyT& key, const ValueT& value, size_t current_position) {
    if (capacity_ == 0) return;

    current_access_index_ = current_position;

    if (data_.find(key) != data_.end()) {
        auto& entry = data_[key];
        entry.data = value;
        
        auto& access   = access_sequence_[key];
        auto& positions = access.accesses;
        size_t& idx    = access.current_index;
        
        while (idx < positions.size() && positions[idx] <= current_position) {
            idx++;
        }
        if (idx < positions.size()) {
            entry.next_use = positions[idx];
        } else {
            entry.next_use = std::numeric_limits<size_t>::max();
        }
        return;
    }

    size_t next_use = std::numeric_limits<size_t>::max();
    auto pattern_it = access_sequence_.find(key);
    
    if (pattern_it != access_sequence_.end()) {
        auto& access    = pattern_it->second;
        auto& positions = access.accesses;
        size_t& idx     = access.current_index;
        
        while (idx < positions.size() && positions[idx] <= current_position) {
            idx++;
        }
        if (idx < positions.size()) {
            next_use = positions[idx];
        }
    }

    if (next_use == std::numeric_limits<size_t>::max()) {
        return;
    }

    if (size_ < capacity_) {
        data_[key] = CacheEntry{value, next_use, key};
        size_++;
        return;
    }

    KeyT   to_remove;
    size_t max_next_use = 0;
    bool   found        = false;
    
    for (const auto& [k, entry] : data_) {
        if (!found || entry.next_use > max_next_use) {
            to_remove    = k;
            max_next_use = entry.next_use;
            found        = true;
        }
    }

    if (found && next_use < max_next_use) {
        data_.erase(to_remove);
        data_[key] = CacheEntry{value, next_use, key};
    }
}



template<typename KeyT, typename ValueT>
bool IdealCache<KeyT, ValueT>::Contains(KeyT key) const {
    return data_.find(key) != data_.end();
}



template<typename KeyT, typename ValueT>
void IdealCache<KeyT, ValueT>::DumpCache() const {
    std::cout << "Index: " << current_access_index_ << std::endl;
    for (const auto& entry : data_) {
        std::cout << "  " << entry.first << " -> " << entry.second.data 
                  << " | Next Use: " << entry.second.next_use << std::endl;
    }
}



template<typename KeyT, typename ValueT>
size_t IdealCache<KeyT, ValueT>::GetCurrentSize() const { 
    return size_; 
}



template<typename KeyT, typename ValueT>
size_t IdealCache<KeyT, ValueT>::GetMaxSize() const { 
    return capacity_; 
}



template<typename KeyT, typename ValueT>
void IdealCache<KeyT, ValueT>::UpdateNextUses() {
    static constexpr size_t MAX_USE = std::numeric_limits<size_t>::max();
    
    for (auto& [key, entry] : data_) {
        auto seq_it = access_sequence_.find(key);
        if (seq_it == access_sequence_.end()) {
            entry.next_use = MAX_USE;
            continue;
        }

        auto& access = seq_it->second;
        size_t& idx = access.current_index;
        const auto& accesses = access.accesses;
        
        while (idx < accesses.size() && accesses[idx] <= current_access_index_) {
            ++idx;
        }

        entry.next_use = (idx < accesses.size()) ? accesses[idx] : MAX_USE;
    }
}



template<typename KeyT, typename ValueT>
void IdealCache<KeyT, ValueT>::Remove() {
    if (data_.empty()) return;

    auto target = std::max_element(data_.begin(), data_.end(), 
        [](const auto& a, const auto& b) {
            return a.second.next_use < b.second.next_use;
        });
    
    data_.erase(target);
    --size_;
}