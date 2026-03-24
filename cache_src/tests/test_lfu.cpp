#include <iostream>
#include <string>

#include "../include/lfu_cache.hpp"

static int passed = 0;
static int failed = 0;

static void check(const std::string& name, bool ok) {
    if (ok) {
        std::cout << "[PASS] " << name << "\n";
        ++passed;
    } else {
        std::cout << "[FAIL] " << name << "\n";
        ++failed;
    }
}

static void test_basic() {
    LfuCache<int, int> c(4);

    check("get on empty returns nullptr", c.Get(1) == nullptr);

    c.Put(1, 100);
    check("get after put", c.Get(1) != nullptr && *c.Get(1) == 100);

    c.Put(1, 200);
    check("put updates existing value", *c.Get(1) == 200);

    c.Put(2, 20);
    c.Put(3, 30);
    c.Put(4, 40);
    check("all 4 keys present", c.Get(2) && c.Get(3) && c.Get(4));
}

static void test_capacity_zero() {
    LfuCache<int, int> c(0);
    c.Put(1, 1);
    check("capacity 0: put is no-op", c.Get(1) == nullptr);
}

static void test_eviction_lfu() {
    LfuCache<int, int> c(2);
    c.Put(1, 10);
    c.Put(2, 20);
    c.Get(1);
    c.Get(1);
    c.Put(3, 30);
    check("high-freq key survives eviction", c.Get(1) != nullptr);
    check("low-freq key evicted", c.Get(2) == nullptr);
    check("new key inserted", c.Get(3) != nullptr);
}

static void test_eviction_tie() {
    LfuCache<int, int> c(2);
    c.Put(1, 10);
    c.Put(2, 20);
    c.Put(3, 30);
    check("tie: older key evicted", c.Get(1) == nullptr);
    check("tie: newer key survives", c.Get(2) != nullptr);
    check("tie: inserted key present", c.Get(3) != nullptr);
}

static void test_refill_after_eviction() {
    LfuCache<int, int> c(2);
    c.Put(1, 10);
    c.Put(2, 20);
    c.Get(2);
    c.Put(3, 30);
    c.Put(1, 11);
    check("re-insert evicted key", c.Get(1) != nullptr && *c.Get(1) == 11);
}

static void test_hit_sequence() {
    LfuCache<int, int> c(3);
    int hits = 0;
    int seq[] = {1, 2, 3, 1, 2, 1, 2, 3, 4, 3};
    for (int x : seq) {
        if (c.Get(x) != nullptr)
            ++hits;
        else
            c.Put(x, x);
    }
    check("hit sequence: 6 hits", hits == 6);
}

int main() {
    std::cout << "=== LfuCache tests ===\n";
    test_basic();
    test_capacity_zero();
    test_eviction_lfu();
    test_eviction_tie();
    test_refill_after_eviction();
    test_hit_sequence();
    std::cout << "===================\n";
    std::cout << "  " << passed << " passed, " << failed << " failed\n";
    std::cout << "===================\n";
    return failed > 0 ? 1 : 0;
}
