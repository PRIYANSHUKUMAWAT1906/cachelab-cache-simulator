#include "cachelab/replacement_policy.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace cachelab {
namespace {

// Shared helper: a (sets x ways) table addressed by (set, way).
template <typename T>
class WayTable {
public:
    WayTable(const CacheConfig& cfg, T init)
        : ways_(static_cast<std::size_t>(cfg.associativity)),
          data_(static_cast<std::size_t>(cfg.num_sets()) * ways_, init) {}
    T& at(std::size_t set, std::size_t way) { return data_[set * ways_ + way]; }
    const T& at(std::size_t set, std::size_t way) const { return data_[set * ways_ + way]; }
    std::size_t ways() const { return ways_; }

private:
    std::size_t ways_;
    std::vector<T> data_;
};

// Evict the way with the smallest `stamp`. Used by LRU (last-use time) and FIFO
// (insertion time); the only difference is when the stamp is refreshed.
class StampPolicy : public ReplacementPolicy {
protected:
    StampPolicy(const CacheConfig& cfg) : stamp_(cfg, 0) {}
    void touch(std::size_t set, std::size_t way) { stamp_.at(set, way) = ++clock_; }

public:
    std::size_t select_victim(std::size_t set) override {
        std::size_t victim = 0;
        std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
        for (std::size_t w = 0; w < stamp_.ways(); ++w) {
            if (stamp_.at(set, w) < best) {
                best = stamp_.at(set, w);
                victim = w;
            }
        }
        return victim;
    }

private:
    WayTable<std::uint64_t> stamp_;
    std::uint64_t clock_ = 0;
};

class LruPolicy final : public StampPolicy {
public:
    explicit LruPolicy(const CacheConfig& cfg) : StampPolicy(cfg) {}
    void on_hit(std::size_t set, std::size_t way) override { touch(set, way); }  // refresh
    void on_insert(std::size_t set, std::size_t way) override { touch(set, way); }
    std::string name() const override { return "LRU"; }
};

class FifoPolicy final : public StampPolicy {
public:
    explicit FifoPolicy(const CacheConfig& cfg) : StampPolicy(cfg) {}
    void on_hit(std::size_t, std::size_t) override {}  // hits never change age
    void on_insert(std::size_t set, std::size_t way) override { touch(set, way); }
    std::string name() const override { return "FIFO"; }
};

// Evict the lowest access count; ties broken by oldest insertion (deterministic).
// The count restarts at 1 when a new block is inserted into a way.
class LfuPolicy final : public ReplacementPolicy {
public:
    explicit LfuPolicy(const CacheConfig& cfg) : freq_(cfg, 0), born_(cfg, 0) {}
    void on_hit(std::size_t set, std::size_t way) override { ++freq_.at(set, way); }
    void on_insert(std::size_t set, std::size_t way) override {
        freq_.at(set, way) = 1;
        born_.at(set, way) = ++clock_;
    }
    std::size_t select_victim(std::size_t set) override {
        std::size_t victim = 0;
        for (std::size_t w = 1; w < freq_.ways(); ++w) {
            const auto fw = freq_.at(set, w), fv = freq_.at(set, victim);
            if (fw < fv || (fw == fv && born_.at(set, w) < born_.at(set, victim))) victim = w;
        }
        return victim;
    }
    std::string name() const override { return "LFU"; }

private:
    WayTable<std::uint64_t> freq_;
    WayTable<std::uint64_t> born_;
    std::uint64_t clock_ = 0;
};

// Rejection sampling avoids modulo bias while keeping seeded results portable.
class RandomPolicy final : public ReplacementPolicy {
public:
    RandomPolicy(const CacheConfig& cfg, std::uint64_t seed)
        : ways_(static_cast<std::size_t>(cfg.associativity)), rng_(seed) {}
    void on_hit(std::size_t, std::size_t) override {}
    void on_insert(std::size_t, std::size_t) override {}
    std::size_t select_victim(std::size_t) override {
        const auto bound = static_cast<std::uint64_t>(ways_);
        const auto threshold = (std::uint64_t{0} - bound) % bound;
        std::uint64_t value;
        do {
            value = rng_();
        } while (value < threshold);
        return static_cast<std::size_t>(value % bound);
    }
    std::string name() const override { return "RANDOM"; }

private:
    std::size_t ways_;
    std::mt19937_64 rng_;
};

}  // namespace

PolicyType parse_policy(const std::string& name) {
    std::string up = name;
    std::transform(up.begin(), up.end(), up.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (up == "LRU") return PolicyType::LRU;
    if (up == "FIFO") return PolicyType::FIFO;
    if (up == "LFU") return PolicyType::LFU;
    if (up == "RANDOM") return PolicyType::RANDOM;
    throw std::invalid_argument("unknown policy '" + name + "' (expected LRU, FIFO, LFU or RANDOM)");
}

std::string to_string(PolicyType type) {
    switch (type) {
        case PolicyType::LRU: return "LRU";
        case PolicyType::FIFO: return "FIFO";
        case PolicyType::LFU: return "LFU";
        case PolicyType::RANDOM: return "RANDOM";
    }
    return "UNKNOWN";
}

std::unique_ptr<ReplacementPolicy> make_policy(PolicyType type, const CacheConfig& cfg,
                                               std::uint64_t seed) {
    switch (type) {
        case PolicyType::LRU: return std::make_unique<LruPolicy>(cfg);
        case PolicyType::FIFO: return std::make_unique<FifoPolicy>(cfg);
        case PolicyType::LFU: return std::make_unique<LfuPolicy>(cfg);
        case PolicyType::RANDOM: return std::make_unique<RandomPolicy>(cfg, seed);
    }
    throw std::invalid_argument("invalid PolicyType");
}

}  // namespace cachelab
