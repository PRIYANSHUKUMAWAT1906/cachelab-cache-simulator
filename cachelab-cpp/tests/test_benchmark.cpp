#include <cstdio>
#include <fstream>
#include <sstream>

#include "cachelab/benchmark.hpp"
#include "test_util.hpp"

using namespace cachelab;

static const Workload* find(const std::vector<Workload>& ws, const std::string& name) {
    for (const auto& w : ws) if (w.name == name) return &w;
    return nullptr;
}

static void test_workloads() {
    CacheConfig cfg{16384, 64, 4};
    auto a = make_standard_workloads(cfg, 42, 8000);
    auto b = make_standard_workloads(cfg, 42, 8000);
    auto c = make_standard_workloads(cfg, 43, 8000);
    CHECK_EQ(a.size(), 6);
    bool same = true, differs = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK_EQ(a[i].addresses.size(), 8000);
        same = same && a[i].addresses == b[i].addresses;
        differs = differs || a[i].addresses != c[i].addresses;
    }
    CHECK(same);      // same seed -> identical workloads
    CHECK(differs);   // different seed -> random workloads change

    // Known-answer: 8-byte stride over 64-byte lines => 7 of every 8 accesses hit.
    auto r = run_one(*find(a, "sequential"), cfg, PolicyType::LRU, 1, 1);
    CHECK_EQ(r.stats.hits, 7000);
    CHECK_EQ(r.stats.misses, 1000);

    // Working set fits: only cold misses (128 lines = half of 256).
    auto fits = run_one(*find(a, "loop_fits"), cfg, PolicyType::LRU, 1, 1);
    CHECK_EQ(fits.stats.misses, 128);
    auto fits_fifo = run_one(*find(a, "loop_fits"), cfg, PolicyType::FIFO, 1, 1);
    CHECK_EQ(fits_fifo.stats.misses, 128);

    // Conflict cycle of ways+1 blocks in one set: LRU and FIFO never hit.
    CHECK_EQ(run_one(*find(a, "conflict"), cfg, PolicyType::LRU, 1, 1).stats.hits, 0);
    CHECK_EQ(run_one(*find(a, "conflict"), cfg, PolicyType::FIFO, 1, 1).stats.hits, 0);
}

static void test_runner_and_csv() {
    CacheConfig cfg{4096, 64, 4};
    auto ws = make_standard_workloads(cfg, 1, 2000);
    std::vector<PolicyType> ps{PolicyType::LRU, PolicyType::FIFO, PolicyType::LFU, PolicyType::RANDOM};
    auto r1 = run_benchmark(ws, cfg, ps, 9, 1);
    auto r2 = run_benchmark(ws, cfg, ps, 9, 1);
    CHECK_EQ(r1.size(), ws.size() * ps.size());
    for (std::size_t i = 0; i < r1.size(); ++i) {   // statistics (not timing) are reproducible
        CHECK_EQ(r1[i].stats.hits, r2[i].stats.hits);
        CHECK_EQ(r1[i].stats.evictions, r2[i].stats.evictions);
        CHECK(r1[i].time_us >= 0.0);
    }

    std::ostringstream os;
    write_csv(os, r1);
    const std::string csv = os.str();
    CHECK(csv.rfind("workload,policy,accesses,hits,misses,evictions,hit_rate,miss_rate,time_us\n", 0) == 0);
    std::size_t lines = 0;
    for (char ch : csv) lines += ch == '\n';
    CHECK_EQ(lines, r1.size() + 1);

    const std::string path = "cachelab_test_out/result.csv";
    write_csv_file(path, r1);
    std::ifstream f(path);
    CHECK(static_cast<bool>(f));
    f.close();
    std::remove(path.c_str());
    std::remove("cachelab_test_out");

    CHECK_THROWS(make_standard_workloads(CacheConfig{1000, 64, 4}, 1, 10));
}

int main() {
    test_workloads();
    test_runner_and_csv();
    return testutil::finish("test_benchmark");
}
