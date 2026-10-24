#include "cachelab/statistics.hpp"

#include <iomanip>
#include <ostream>

namespace cachelab {

std::ostream& operator<<(std::ostream& os, const Statistics& s) {
    const auto flags = os.flags();
    const auto prec = os.precision();
    os << "accesses : " << s.accesses << '\n'
       << "hits     : " << s.hits << '\n'
       << "misses   : " << s.misses << '\n'
       << "evictions: " << s.evictions << '\n'
       << std::fixed << std::setprecision(2)
       << "hit rate : " << s.hit_rate() * 100.0 << " %\n"
       << "miss rate: " << s.miss_rate() * 100.0 << " %\n";
    os.flags(flags);
    os.precision(prec);
    return os;
}

}  // namespace cachelab
