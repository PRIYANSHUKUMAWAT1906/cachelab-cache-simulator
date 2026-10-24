#pragma once
#include <cstdint>

namespace cachelab {

struct CacheLine {
    bool valid = false;
    std::uint64_t tag = 0;
};

}  // namespace cachelab
