#include <iostream>
#include <vector>
#include <unordered_map>

#include "../include/ideal_cache.hpp"



size_t RunIdeal();



int main() {
    size_t hit_count = RunIdeal();
    std::cout << hit_count << std::endl;
    return 0;
}



size_t RunIdeal() {
    size_t cache_size, element_count;
    if (!(std::cin >> cache_size >> element_count)) {
        std::cerr << "Error reading cache parameters" << std::endl;
        return 0;
    }

    IdealCache<int, int> cache(cache_size);
    std::vector<int> elements;
    elements.reserve(element_count);
    std::unordered_map<int, std::vector<size_t>> access_map;

    for (size_t i = 0; i < element_count; ++i) {
        int element;
        if (!(std::cin >> element)) {
            std::cerr << "Error reading element " << i + 1 << std::endl;
            break;
        }
        elements.push_back(element);
        access_map[element].push_back(i);
    }

    for (auto& [key, positions] : access_map) {
        cache.LoadAccessPattern(key, positions);
    }

    size_t hit_count = 0;
    for (size_t i = 0; i < element_count; ++i) {
        int element = elements[i];
        
        if (cache.Contains(element)) {
            ++hit_count;
            cache.Put(element, element, i);
        } else {
            cache.Put(element, element, i);
        }
        
        //cache.DumpCache();
    }

    return hit_count;
}