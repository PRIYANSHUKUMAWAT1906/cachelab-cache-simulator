#pragma once
// Minimal dependency-free test helpers (no external framework needed).
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include "cachelab/cache_simulator.hpp"

namespace testutil {

inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

inline int finish(const char* suite) {
    std::cout << suite << ": " << checks() - failures() << "/" << checks() << " checks passed\n";
    return failures() == 0 ? 0 : 1;
}

// Cache with `ways` ways and exactly one set (64-byte lines).
inline cachelab::CacheConfig one_set(std::uint64_t ways) {
    return cachelab::CacheConfig{64 * ways, 64, ways};
}

// Replays block IDs (block * line_size = byte address) through a fresh cache.
inline cachelab::Statistics run_blocks(const cachelab::CacheConfig& cfg, cachelab::PolicyType p,
                                       const std::vector<std::uint64_t>& blocks,
                                       std::uint64_t seed = 1) {
    cachelab::CacheSimulator sim(cfg, p, seed);
    for (auto b : blocks) sim.access(b * cfg.line_size);
    return sim.stats();
}

}  // namespace testutil

#define CHECK(cond)                                                                  \
    do {                                                                             \
        ++testutil::checks();                                                        \
        if (!(cond)) {                                                               \
            ++testutil::failures();                                                  \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
        }                                                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                               \
    do {                                                                             \
        ++testutil::checks();                                                        \
        const auto va_ = (a);                                                        \
        const std::decay_t<decltype(va_)> vb_ = static_cast<std::decay_t<decltype(va_)>>(b);                     \
        if (!(va_ == vb_)) {                                                         \
            ++testutil::failures();                                                  \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #a " == " #b \
                      << "  (" << va_ << " vs " << vb_ << ")\n";                     \
        }                                                                            \
    } while (0)

#define CHECK_THROWS(expr)                                                           \
    do {                                                                             \
        ++testutil::checks();                                                        \
        bool threw_ = false;                                                         \
        try { (void)(expr); } catch (const std::exception&) { threw_ = true; }       \
        if (!threw_) {                                                               \
            ++testutil::failures();                                                  \
            std::cerr << __FILE__ << ":" << __LINE__ << ": expected exception: " #expr "\n"; \
        }                                                                            \
    } while (0)
