#include <algorithm>
#include <list>
#include <random>

#include "test_util.hpp"

using namespace cachelab;
using testutil::one_set;
using testutil::run_blocks;

static void test_lru() {
    auto s = run_blocks(one_set(2), PolicyType::LRU, {0, 1, 0, 2, 0});
    CHECK_EQ(s.misses, 3);
    CHECK_EQ(s.hits, 2);
    CHECK_EQ(s.evictions, 1);
    // 0,1,0,2 evicts 1 (LRU), so 1 must miss afterwards and evict 2.
    auto t = run_blocks(one_set(2), PolicyType::LRU, {0, 1, 0, 2, 1});
    CHECK_EQ(t.hits, 1);
    CHECK_EQ(t.misses, 4);
}

static void test_fifo() {
    auto s = run_blocks(one_set(2), PolicyType::FIFO, {0, 1, 0, 2, 0});
    CHECK_EQ(s.misses, 4);
    CHECK_EQ(s.hits, 1);
    CHECK_EQ(s.evictions, 2);
    // FIFO evicts 0 at access #4 even though 0 was just hit; so 1 survives.
    auto t = run_blocks(one_set(2), PolicyType::FIFO, {0, 1, 0, 2, 1});
    CHECK_EQ(t.hits, 2);  // the re-reference of 0 and the final 1
}

static void test_lfu() {
    // freq(0)=2, freq(1)=1 -> block 2 evicts 1; final 0 hits.
    auto s = run_blocks(one_set(2), PolicyType::LFU, {0, 0, 1, 2, 0});
    CHECK_EQ(s.misses, 3);
    CHECK_EQ(s.hits, 2);
    CHECK_EQ(s.evictions, 1);

    // Tie on frequency (both 2) -> evict the oldest *inserted* (block 0).
    // LRU would evict block 1 here, so the final access to 1 distinguishes them.
    auto tie = run_blocks(one_set(2), PolicyType::LFU, {0, 1, 1, 0, 2, 1});
    CHECK_EQ(tie.misses, 3);
    CHECK_EQ(tie.hits, 3);
    auto lru = run_blocks(one_set(2), PolicyType::LRU, {0, 1, 1, 0, 2, 1});
    CHECK_EQ(lru.hits, 2);

    // Frequency restarts for a newly inserted block: 0 (freq 3) stays, 1 and 2 churn.
    auto churn = run_blocks(one_set(2), PolicyType::LFU, {0, 0, 0, 1, 2, 1, 2});
    CHECK_EQ(churn.hits, 2);  // only the repeated 0s hit; 1 and 2 keep evicting each other
}

static void test_random() {
    std::mt19937_64 rng(99);
    std::vector<std::uint64_t> blocks;
    for (int i = 0; i < 4000; ++i) blocks.push_back(rng() % 300);
    CacheConfig cfg{4096, 64, 4};

    auto a = run_blocks(cfg, PolicyType::RANDOM, blocks, 123);
    auto b = run_blocks(cfg, PolicyType::RANDOM, blocks, 123);
    CHECK_EQ(a.hits, b.hits);
    CHECK_EQ(a.misses, b.misses);
    CHECK_EQ(a.evictions, b.evictions);

    // Different seeds should (for this trace) give a different eviction pattern.
    auto c = run_blocks(cfg, PolicyType::RANDOM, blocks, 456);
    CHECK(a.hits != c.hits || a.misses != c.misses);

    // Single-way cache: no choice exists, so RANDOM == every other policy.
    CacheConfig dm{1024, 64, 1};
    auto r = run_blocks(dm, PolicyType::RANDOM, blocks, 5);
    for (auto p : {PolicyType::LRU, PolicyType::FIFO, PolicyType::LFU}) {
        auto o = run_blocks(dm, p, blocks);
        CHECK_EQ(o.hits, r.hits);
        CHECK_EQ(o.evictions, r.evictions);
    }
}

// Independent list-based LRU model compared against the simulator.
static void test_lru_against_reference_model() {
    const std::uint64_t sets = 8, ways = 4;
    CacheConfig cfg{sets * ways * 64, 64, ways};
    std::mt19937_64 rng(2024);
    std::vector<std::list<std::uint64_t>> ref(sets);  // front = MRU
    CacheSimulator sim(cfg, PolicyType::LRU);
    std::uint64_t ref_hits = 0, ref_evict = 0;
    for (int i = 0; i < 20000; ++i) {
        const std::uint64_t block = rng() % 64;
        auto& l = ref[block % sets];
        auto it = std::find(l.begin(), l.end(), block);
        if (it != l.end()) {
            ++ref_hits;
            l.erase(it);
        } else if (l.size() == ways) {
            l.pop_back();
            ++ref_evict;
        }
        l.push_front(block);
        sim.access(block * 64);
    }
    CHECK_EQ(sim.stats().hits, ref_hits);
    CHECK_EQ(sim.stats().evictions, ref_evict);
}

static void test_parse_and_names() {
    CHECK(parse_policy("lru") == PolicyType::LRU);
    CHECK(parse_policy("Fifo") == PolicyType::FIFO);
    CHECK(parse_policy("LFU") == PolicyType::LFU);
    CHECK(parse_policy("random") == PolicyType::RANDOM);
    CHECK_THROWS(parse_policy("mru"));
    CHECK_THROWS(parse_policy(""));
    CHECK(to_string(PolicyType::RANDOM) == "RANDOM");
    CHECK(make_policy(PolicyType::LFU, one_set(2))->name() == "LFU");
}

int main() {
    test_lru();
    test_fifo();
    test_lfu();
    test_random();
    test_lru_against_reference_model();
    test_parse_and_names();
    return testutil::finish("test_policies");
}
