#include <iostream>

#include "../include/lfu_cache.hpp"



size_t  RunLfu();
//void    RunComparison(int &total_hits_lfu, int &total_hits_ideal);



int main() {
    size_t hit_count    = 0;

    //std::cout << "\n=== Running Standard LFU Test ===\n";
    hit_count = RunLfu();
    std::cout << ">>" << hit_count << std::endl;

    return 0;
}



size_t RunLfu() 
{
    size_t cache_size = 0;
    size_t element_count = 0;
    size_t hit_count = 0;

    if (!(std::cin >> cache_size >> element_count)) {
        std::cout << "Error reading cache parameters " << std::endl;

        return 0;
    }

    LfuCache<int, int> cache(cache_size);

    for (size_t i = 0; i < element_count; ++i) {
        //cache.DumpCache();
        int element = 0;
        if (!(std::cin >> element)) {
            std::cout << "Error reading element " << i + 1 << std::endl;
            
            break;
        }

        if (cache.Get(element) != nullptr) 
            ++hit_count;
        else 
            cache.Put(element, element);
    }
    return hit_count;
}




/*

void RunComparison(int &total_hits_lfu, int &total_hits_ideal) {


    std::ifstream file("../test/test.txt");
    if (!file.is_open()) {
        std::cout << "Error opening file: test/test.txt" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::cout << "Test case: " << line << std::endl;
        
        std::istringstream iss(line);
        size_t cache_size = 0;
        size_t element_count = 0;
        
        if (!(iss >> cache_size >> element_count)) {
            std::cout << "Error reading cache parameters!" << std::endl;
            continue;
        }

        std::vector<int> elements;
        for (size_t i = 0; i < element_count; ++i) {
            int element = 0;
            if (!(iss >> element)) {
                std::cout << "Error reading element " << i + 1 << std::endl;
                break;
            }
            elements.push_back(element);
        }

        // LFU
        LfuCache<int, int> lfu_cache(cache_size);
        size_t lfu_hits = 0;
        for (int element : elements) {
            if (lfu_cache.Get(element) != nullptr) 
                ++lfu_hits;
            else 
                lfu_cache.Put(element, element);
        }

        // Ideal
        IdealCache<int, int> ideal_cache(cache_size);
        std::unordered_map<int, std::vector<size_t>> access_map;
        for (size_t i = 0; i < elements.size(); ++i) {
            access_map[elements[i]].push_back(i);
        }
        for (auto& [key, positions] : access_map) {
            ideal_cache.LoadAccessPattern(key, positions);
        }
        
        size_t ideal_hits = 0;
        for (int element : elements) {
            if (ideal_cache.Get(element) != nullptr) 
                ++ideal_hits;
            else 
                ideal_cache.Put(element, element);
        }

        std::cout << "LFU hits: " << lfu_hits << std::endl;
        std::cout << "Ideal hits: " << ideal_hits << std::endl;
        std::cout << "---" << std::endl;

        total_hits_lfu   += lfu_hits;
        total_hits_ideal += ideal_hits;
    }
    file.close();
}

*/