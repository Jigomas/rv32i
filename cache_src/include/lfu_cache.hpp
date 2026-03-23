#pragma once

#include <iostream>
#include <list>
#include <unordered_map>



template<typename KeyT, typename ValueT>
class LfuCache {
public:
	explicit LfuCache(size_t capacity = 0);
	
	auto begin() { return data_.begin(); }
	auto end()   { return data_.end(); }

	ValueT* Get(KeyT key);
	void    Put(const KeyT& key, const ValueT& value);
	void    DumpCache() const;

private:
	void Remove();
	void Promote(KeyT key);

	std::unordered_map<KeyT, ValueT> 								data_;
	std::unordered_map<KeyT, size_t> 								frequency_;
	std::unordered_map<KeyT, typename std::list<KeyT>::iterator> 	iterator_map_;
	std::unordered_map<size_t, std::list<KeyT>> 					frequency_buckets_;

	size_t capacity_;
	size_t size_;
	size_t min_frequency_;
};



template<typename KeyT, typename ValueT>
LfuCache<KeyT, ValueT>::LfuCache(size_t capacity)
	: capacity_(capacity), size_(0), min_frequency_(0) {}

template<typename KeyT, typename ValueT>
ValueT* LfuCache<KeyT, ValueT>::Get(KeyT key) {
	auto it = data_.find(key);
	if (it == data_.end()) {
		return nullptr;
	}
	Promote(key);
	return &(it->second);
}



template<typename KeyT, typename ValueT>
void LfuCache<KeyT, ValueT>::Put(const KeyT& key, const ValueT& value) {
	if (capacity_ == 0) return;

	auto it = data_.find(key);
	if (it != data_.end()) {
		it->second = value;
		Promote(key);
        
		return;
	}

	if (size_ >= capacity_) {
		Remove();
	}

	data_[key] = value;
	frequency_[key] = 1;
	frequency_buckets_[1].push_front(key);
	iterator_map_[key] = frequency_buckets_[1].begin();
	min_frequency_ = 1;
	++size_;
}



template<typename KeyT, typename ValueT>
void LfuCache<KeyT, ValueT>::DumpCache() const {
	std::cout << "Capacity: " << capacity_ << ", Size: " << size_ << std::endl;
	std::cout << "Data contents:" << std::endl;
	for (const auto& entry : data_) {
		std::cout << "  " << entry.first << " -> " << entry.second << std::endl;
	}
}



template<typename KeyT, typename ValueT>
void LfuCache<KeyT, ValueT>::Remove() {
    if (frequency_buckets_.empty()) return;

    auto min_bucket_it = frequency_buckets_.find(min_frequency_);
    if (min_bucket_it == frequency_buckets_.end()) {
        min_frequency_ = frequency_buckets_.begin()->first;
        for (const auto& bucket : frequency_buckets_) {
            if (bucket.first < min_frequency_) {
                min_frequency_ = bucket.first;
            }
        }
        min_bucket_it = frequency_buckets_.find(min_frequency_);
    }

    auto& min_bucket = min_bucket_it->second;
    KeyT key_to_remove = min_bucket.back();
    min_bucket.pop_back();

    if (min_bucket.empty()) {
        frequency_buckets_.erase(min_frequency_);
    }

    data_.erase(key_to_remove);
    frequency_.erase(key_to_remove);
    iterator_map_.erase(key_to_remove);
    --size_;
}



template<typename KeyT, typename ValueT>
void LfuCache<KeyT, ValueT>::Promote(KeyT key) {
    size_t old_freq = frequency_[key];
    
    auto old_bucket_it = frequency_buckets_.find(old_freq);
    if (old_bucket_it != frequency_buckets_.end()) {
        auto& old_bucket = old_bucket_it->second;
        old_bucket.erase(iterator_map_[key]);
        
        if (old_bucket.empty()) {
            frequency_buckets_.erase(old_freq);
            if (old_freq == min_frequency_) {
                min_frequency_ = frequency_buckets_.empty() ? 1 : 
                                frequency_buckets_.begin()->first;
                for (const auto& bucket : frequency_buckets_) {
                    if (bucket.first < min_frequency_) {
                        min_frequency_ = bucket.first;
                    }
                }
            }
        }
    }

    size_t new_freq = old_freq + 1;
    frequency_[key] = new_freq;
    frequency_buckets_[new_freq].push_front(key);
    iterator_map_[key] = frequency_buckets_[new_freq].begin();
}