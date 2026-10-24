#include <list>
#include <random>

#include "test_util.hpp"

using namespace cachelab;
using testutil::one_set;
using testutil::run_blocks;

static void test_config() {
    CacheConfig c{16384, 64, 4};
    c.validate();
    CHECK_EQ(c.num_sets(), 64);
    CHECK_EQ(c.num_lines(), 256);

    CHECK_THROWS((CacheConfig{0, 64, 4}).validate());
    CHECK_THROWS((CacheConfig{1024, 0, 4}).validate());
    CHECK_THROWS((CacheConfig{1024, 64, 0}).validate());
    CHECK_THROWS((CacheConfig{1024, 48, 1}).validate());      // line not power of two
    CHECK_THROWS((CacheConfig{1000, 64, 4}).validate());      // not divisible
    CHECK_THROWS((CacheConfig{1024, 64, 32}).validate());     // more ways than lines
    CHECK_THROWS((CacheConfig{32, 64, 1}).validate());        // line > capacity
    CHECK_THROWS((CacheConfig{3 * 64, 64, 2}).validate());    // 192 % 128 != 0
    CHECK_THROWS(CacheSimulator(CacheConfig{1000, 64, 4}, PolicyType::LRU));
}

static void test_address_mapping() {
    CacheSimulator sim(CacheConfig{16384, 64, 4}, PolicyType::LRU);
    // block = addr/64, set = block % 64, tag = block / 64
    CHECK_EQ(sim.block_of(0), 0);
    CHECK_EQ(sim.block_of(63), 0);
    CHECK_EQ(sim.block_of(64), 1);
    CHECK_EQ(sim.set_of(64 * 5), 5);
    CHECK_EQ(sim.tag_of(64 * 5), 0);
    CHECK_EQ(sim.set_of(64 * 64), 0);   // wraps to set 0
    CHECK_EQ(sim.tag_of(64 * 64), 1);
    CHECK_EQ(sim.set_of(64 * 130), 2);
    CHECK_EQ(sim.tag_of(64 * 130), 2);
    CHECK_EQ(sim.set_of(0xFFFFFFFFFFFFFFFFull), 63);  // extreme address stays in range
}

static void test_basic_behaviour() {
    for (auto p : {PolicyType::LRU, PolicyType::FIFO, PolicyType::LFU, PolicyType::RANDOM}) {
        auto cold = run_blocks(one_set(2), p, {0, 1, 2, 3});
        CHECK_EQ(cold.misses, 4);
        CHECK_EQ(cold.hits, 0);

        auto rep = run_blocks(one_set(2), p, {0, 0, 0});
        CHECK_EQ(rep.misses, 1);
        CHECK_EQ(rep.hits, 2);
        CHECK_EQ(rep.evictions, 0);  // hits never evict

        // Two blocks into two free ways: no eviction.
        auto free_way = run_blocks(one_set(2), p, {0, 1});
        CHECK_EQ(free_way.misses, 2);
        CHECK_EQ(free_way.evictions, 0);

        // Third distinct block forces exactly one eviction.
        auto full = run_blocks(one_set(2), p, {0, 1, 2});
        CHECK_EQ(full.evictions, 1);

        // Empty trace.
        auto empty = run_blocks(one_set(2), p, {});
        CHECK_EQ(empty.accesses, 0);
        CHECK_EQ(empty.hit_rate(), 0.0);
        CHECK_EQ(empty.miss_rate(), 0.0);
    }
}

static void test_spatial_locality_and_sets() {
    CacheSimulator sim(CacheConfig{16384, 64, 4}, PolicyType::LRU);
    CHECK(!sim.access(0));    // cold miss
    CHECK(sim.access(63));    // same 64-byte line -> hit
    CHECK(!sim.access(64));   // next line -> miss
    CHECK(sim.access(0));

    // Blocks in different sets never evict each other.
    CacheSimulator s2(CacheConfig{256, 64, 1}, PolicyType::LRU);  // direct-mapped, 4 sets
    for (int round = 0; round < 3; ++round)
        for (std::uint64_t b = 0; b < 4; ++b) s2.access(b * 64);
    CHECK_EQ(s2.stats().misses, 4);
    CHECK_EQ(s2.stats().hits, 8);
    CHECK_EQ(s2.stats().evictions, 0);

    // Blocks 0 and 4 conflict in set 0 of a 4-set direct-mapped cache.
    CacheSimulator s3(CacheConfig{256, 64, 1}, PolicyType::LRU);
    for (int i = 0; i < 3; ++i) { s3.access(0); s3.access(4 * 64); }
    CHECK_EQ(s3.stats().hits, 0);
    CHECK_EQ(s3.stats().evictions, 5);
}

static void test_invariants_all_policies() {
    std::mt19937_64 rng(7);
    std::vector<std::uint64_t> blocks;
    for (int i = 0; i < 5000; ++i) blocks.push_back(rng() % 200);
    CacheConfig cfg{4096, 64, 4};
    for (auto p : {PolicyType::LRU, PolicyType::FIFO, PolicyType::LFU, PolicyType::RANDOM}) {
        auto s = run_blocks(cfg, p, blocks);
        CHECK_EQ(s.accesses, 5000);
        CHECK_EQ(s.hits + s.misses, s.accesses);
        CHECK(s.evictions <= s.misses);
        CHECK(s.hit_rate() + s.miss_rate() > 0.999999 && s.hit_rate() + s.miss_rate() < 1.000001);
        // Misses beyond the cache's line count must have evicted something.
        CHECK_EQ(s.evictions + cfg.num_lines() >= s.misses ? 1 : 0, 1);
    }
}

static void test_reset() {
    CacheSimulator sim(one_set(2), PolicyType::LRU);
    sim.access(0);
    sim.access(0);
    sim.reset();
    CHECK_EQ(sim.stats().accesses, 0);
    CHECK(!sim.access(0));  // cold again after reset
}

int main() {
    test_config();
    test_address_mapping();
    test_basic_behaviour();
    test_spatial_locality_and_sets();
    test_invariants_all_policies();
    test_reset();
    return testutil::finish("test_cache");
}
