#include "cachelab/cache_simulator.hpp"

namespace cachelab {

CacheSimulator::CacheSimulator(const CacheConfig& cfg, PolicyType policy, std::uint64_t seed)
    : cfg_(cfg), type_(policy), seed_(seed) {
    cfg_.validate();
    num_sets_ = cfg_.num_sets();
    ways_ = static_cast<std::size_t>(cfg_.associativity);
    line_shift_ = 0;
    while ((std::uint64_t{1} << line_shift_) < cfg_.line_size) ++line_shift_;
    reset();
}

void CacheSimulator::reset() {
    lines_.assign(static_cast<std::size_t>(num_sets_) * ways_, CacheLine{});
    policy_ = make_policy(type_, cfg_, seed_);
    stats_.reset();
}

bool CacheSimulator::access(std::uint64_t address) {
    const std::uint64_t block = block_of(address);
    const std::size_t set = static_cast<std::size_t>(block % num_sets_);
    const std::uint64_t tag = block / num_sets_;
    CacheLine* row = &lines_[set * ways_];

    ++stats_.accesses;

    // 1. Lookup.
    for (std::size_t w = 0; w < ways_; ++w) {
        if (row[w].valid && row[w].tag == tag) {
            ++stats_.hits;
            policy_->on_hit(set, w);
            return true;
        }
    }

    // 2. Miss: prefer a free way (not an eviction), else ask the policy for a victim.
    ++stats_.misses;
    std::size_t way = ways_;
    for (std::size_t w = 0; w < ways_; ++w) {
        if (!row[w].valid) {
            way = w;
            break;
        }
    }
    if (way == ways_) {
        way = policy_->select_victim(set);
        ++stats_.evictions;
    }

    row[way].valid = true;
    row[way].tag = tag;
    policy_->on_insert(set, way);
    return false;
}

}  // namespace cachelab
