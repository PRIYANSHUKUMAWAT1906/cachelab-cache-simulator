#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "cachelab/cache_config.hpp"
#include "cachelab/cache_line.hpp"
#include "cachelab/replacement_policy.hpp"
#include "cachelab/statistics.hpp"

namespace cachelab {

// Single-level, set-associative cache. The mapping logic lives here once;
// replacement decisions are delegated to a ReplacementPolicy.
class CacheSimulator {
public:
    CacheSimulator(const CacheConfig& cfg, PolicyType policy, std::uint64_t seed = 1);

    // Simulates one byte-address access. Returns true on a hit.
    bool access(std::uint64_t address);

    const Statistics& stats() const { return stats_; }
    const CacheConfig& config() const { return cfg_; }
    PolicyType policy_type() const { return type_; }
    void reset();

    // Address decomposition (exposed for tests and tooling).
    std::uint64_t block_of(std::uint64_t address) const { return address >> line_shift_; }
    std::uint64_t set_of(std::uint64_t address) const { return block_of(address) % num_sets_; }
    std::uint64_t tag_of(std::uint64_t address) const { return block_of(address) / num_sets_; }

private:
    CacheConfig cfg_;
    PolicyType type_;
    std::uint64_t seed_;
    std::uint64_t num_sets_ = 0;
    std::size_t ways_ = 0;
    unsigned line_shift_ = 0;
    std::vector<CacheLine> lines_;  // num_sets_ * ways_, row-major by set
    std::unique_ptr<ReplacementPolicy> policy_;
    Statistics stats_;
};

}  // namespace cachelab
