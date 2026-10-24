#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "cachelab/cache_config.hpp"

namespace cachelab {

enum class PolicyType { LRU, FIFO, LFU, RANDOM };

// Case-insensitive; throws std::invalid_argument on unknown names.
PolicyType parse_policy(const std::string& name);
std::string to_string(PolicyType type);

// Each policy owns its per-line metadata. The simulator only reports events:
//   on_insert     : a block was placed into (set, way)  (after a miss)
//   on_hit        : the block in (set, way) was re-referenced
//   select_victim : every way in `set` is valid; pick the way to evict
class ReplacementPolicy {
public:
    virtual ~ReplacementPolicy() = default;
    virtual void on_hit(std::size_t set, std::size_t way) = 0;
    virtual void on_insert(std::size_t set, std::size_t way) = 0;
    virtual std::size_t select_victim(std::size_t set) = 0;
    virtual std::string name() const = 0;
};

std::unique_ptr<ReplacementPolicy> make_policy(PolicyType type, const CacheConfig& cfg,
                                               std::uint64_t seed = 1);

}  // namespace cachelab
