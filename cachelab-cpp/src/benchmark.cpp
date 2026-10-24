#include "cachelab/benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <random>
#include <stdexcept>

#include "cachelab/cache_simulator.hpp"

namespace cachelab {
namespace {

void run_workload(CacheSimulator& sim, const Workload& workload) {
    for (std::uint64_t address : workload.addresses) sim.access(address);
}

}  // namespace

std::vector<Workload> make_standard_workloads(const CacheConfig& cfg, std::uint64_t seed,
                                              std::size_t n) {
    cfg.validate();
    const std::uint64_t line = cfg.line_size;
    const std::uint64_t lines = cfg.num_lines();
    const std::uint64_t sets = cfg.num_sets();
    const std::uint64_t ways = cfg.associativity;
    std::mt19937_64 rng(seed);

    std::vector<Workload> out;
    auto make = [&](const char* name, auto gen) {
        Workload w;
        w.name = name;
        w.addresses.reserve(n);
        for (std::size_t i = 0; i < n; ++i) w.addresses.push_back(gen(static_cast<std::uint64_t>(i)));
        out.push_back(std::move(w));
    };

    make("sequential", [&](std::uint64_t i) { return i * 8; });

    const std::uint64_t fit = std::max<std::uint64_t>(1, lines / 2);
    make("loop_fits", [&](std::uint64_t i) { return (i % fit) * line; });

    const std::uint64_t over = lines + std::max<std::uint64_t>(1, lines / 4);
    make("loop_exceeds", [&](std::uint64_t i) { return (i % over) * line; });

    make("random", [&](std::uint64_t) { return (rng() % (8 * lines)) * line; });

    const std::uint64_t hot = std::max<std::uint64_t>(1, lines / 8);
    make("hotset", [&](std::uint64_t) {
        if (rng() % 100 < 80) return (rng() % hot) * line;
        return (hot + rng() % (16 * lines)) * line;
    });

    make("conflict", [&](std::uint64_t i) { return ((i % (ways + 1)) * sets) * line; });

    return out;
}

BenchmarkResult run_one(const Workload& w, const CacheConfig& cfg, PolicyType policy,
                        std::uint64_t seed, int repeats) {
    if (repeats < 1) repeats = 1;
    constexpr int warmup_runs = 1;
    for (int rep = 0; rep < warmup_runs; ++rep) {
        CacheSimulator sim(cfg, policy, seed);
        run_workload(sim, w);
    }

    CacheSimulator stats_sim(cfg, policy, seed);
    run_workload(stats_sim, w);

    std::vector<double> times;
    times.reserve(static_cast<std::size_t>(repeats));
    for (int rep = 0; rep < repeats; ++rep) {
        CacheSimulator sim(cfg, policy, seed);
        const auto t0 = std::chrono::steady_clock::now();
        run_workload(sim, w);
        const auto t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }

    std::sort(times.begin(), times.end());
    const std::size_t middle = times.size() / 2;
    const double median = times.size() % 2 == 0
        ? (times[middle - 1] + times[middle]) / 2.0
        : times[middle];
    BenchmarkResult r{w.name, policy, stats_sim.stats(), median};
    return r;
}

std::vector<BenchmarkResult> run_benchmark(const std::vector<Workload>& workloads,
                                           const CacheConfig& cfg,
                                           const std::vector<PolicyType>& policies,
                                           std::uint64_t seed, int repeats) {
    std::vector<BenchmarkResult> results;
    for (const auto& w : workloads)
        for (PolicyType p : policies) results.push_back(run_one(w, cfg, p, seed, repeats));
    return results;
}

void write_csv(std::ostream& os, const std::vector<BenchmarkResult>& results) {
    os << "workload,policy,accesses,hits,misses,evictions,hit_rate,miss_rate,time_us\n";
    os << std::fixed;
    for (const auto& r : results) {
        os << r.workload << ',' << to_string(r.policy) << ',' << r.stats.accesses << ','
           << r.stats.hits << ',' << r.stats.misses << ',' << r.stats.evictions << ','
           << std::setprecision(6) << r.stats.hit_rate() << ',' << r.stats.miss_rate() << ','
           << std::setprecision(1) << r.time_us << '\n';
    }
}

void write_csv_file(const std::string& path, const std::vector<BenchmarkResult>& results) {
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot write '" + path + "'");
    write_csv(f, results);
}

void print_results(std::ostream& os, const std::vector<BenchmarkResult>& results) {
    std::size_t wid = 10;
    for (const auto& r : results) wid = std::max(wid, r.workload.size());
    wid += 2;
    os << std::left << std::setw(static_cast<int>(wid)) << "workload" << std::setw(8) << "policy" << std::right
       << std::setw(10) << "accesses" << std::setw(10) << "hits" << std::setw(10) << "misses"
       << std::setw(10) << "evicts" << std::setw(10) << "hit %" << std::setw(12) << "time_us"
       << '\n';
    for (const auto& r : results) {
        os << std::left << std::setw(static_cast<int>(wid)) << r.workload << std::setw(8) << to_string(r.policy)
           << std::right << std::setw(10) << r.stats.accesses << std::setw(10) << r.stats.hits
           << std::setw(10) << r.stats.misses << std::setw(10) << r.stats.evictions
           << std::setw(10) << std::fixed << std::setprecision(2) << r.stats.hit_rate() * 100.0
           << std::setw(12) << std::setprecision(1) << r.time_us << '\n';
    }
}

}  // namespace cachelab
