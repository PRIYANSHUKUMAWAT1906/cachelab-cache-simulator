#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace cachelab {

// Geometry of a single-level set-associative cache.
struct CacheConfig {
    std::uint64_t capacity_bytes = 16384;
    std::uint64_t line_size = 64;
    std::uint64_t associativity = 4;

    std::uint64_t num_sets() const { return capacity_bytes / (line_size * associativity); }
    std::uint64_t num_lines() const { return capacity_bytes / line_size; }

    // Throws std::invalid_argument if the geometry is not realisable.
    void validate() const {
        if (capacity_bytes == 0 || line_size == 0 || associativity == 0)
            throw std::invalid_argument("capacity, line size and associativity must be > 0");
        if ((line_size & (line_size - 1)) != 0)
            throw std::invalid_argument("line size must be a power of two");
        if (line_size > capacity_bytes)
            throw std::invalid_argument("line size must not exceed capacity");
        if (associativity > capacity_bytes / line_size)
            throw std::invalid_argument("associativity exceeds the number of cache lines");
        if (capacity_bytes % (line_size * associativity) != 0)
            throw std::invalid_argument(
                "capacity must be divisible by (line size x associativity)");
    }
};

}  // namespace cachelab
