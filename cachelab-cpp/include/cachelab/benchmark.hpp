#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "cachelab/cache_config.hpp"
#include "cachelab/replacement_policy.hpp"
#include "cachelab/statistics.hpp"

namespace cachelab {

struct Workload {
    std::string name;
    std::vector<std::uint64_t> addresses;
};

struct BenchmarkResult {
    std::string workload;
    PolicyType policy;
    Statistics stats;
    double time_us;  // median wall-clock time across measured repetitions
};

// Deterministic synthetic workloads, sized relative to the cache geometry:
//   sequential    - 8-byte stride scan (spatial locality only)
//   loop_fits     - cyclic reuse of 50% of the cache's lines
//   loop_exceeds  - cyclic reuse of 125% of the cache's lines
//   random        - uniform over 8x the cache capacity
//   hotset        - 80% of accesses to a small hot set, 20% to a large cold range
//   conflict      - cycles through (ways+1) blocks that all map to set 0
std::vector<Workload> make_standard_workloads(const CacheConfig& cfg, std::uint64_t seed,
                                              std::size_t accesses);

BenchmarkResult run_one(const Workload& w, const CacheConfig& cfg, PolicyType policy,
                        std::uint64_t seed, int repeats = 3);

std::vector<BenchmarkResult> run_benchmark(const std::vector<Workload>& workloads,
                                           const CacheConfig& cfg,
                                           const std::vector<PolicyType>& policies,
                                           std::uint64_t seed, int repeats = 3);

// Header: workload,policy,accesses,hits,misses,evictions,hit_rate,miss_rate,time_us
void write_csv(std::ostream& os, const std::vector<BenchmarkResult>& results);
void write_csv_file(const std::string& path, const std::vector<BenchmarkResult>& results);
void print_results(std::ostream& os, const std::vector<BenchmarkResult>& results);

}  // namespace cachelab
