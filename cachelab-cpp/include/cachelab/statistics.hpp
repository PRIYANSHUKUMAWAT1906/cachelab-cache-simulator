#pragma once
#include <cstdint>
#include <iosfwd>

namespace cachelab {

struct Statistics {
    std::uint64_t accesses = 0;
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;

    // Both rates are 0.0 for an empty trace (no division by zero).
    double hit_rate() const { return accesses ? static_cast<double>(hits) / accesses : 0.0; }
    double miss_rate() const { return accesses ? static_cast<double>(misses) / accesses : 0.0; }
    void reset() { *this = Statistics{}; }
};

std::ostream& operator<<(std::ostream& os, const Statistics& s);

}  // namespace cachelab
