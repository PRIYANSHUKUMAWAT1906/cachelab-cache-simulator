#include <cctype>
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cachelab/benchmark.hpp"
#include "cachelab/trace_reader.hpp"

using namespace cachelab;

namespace {

const char* kUsage = R"(CacheLab - trace-driven cache replacement simulator

Usage:
  cachelab --trace FILE   [options]     simulate an address trace
  cachelab --benchmark    [options]     run built-in synthetic workloads

Cache geometry:
  --capacity N        cache size in bytes; K/M/G suffixes allowed   (default 16K)
  --line-size N       line size in bytes, power of two              (default 64)
  --associativity N   ways per set                                  (default 4)

Policies:
  --policy NAME       LRU | FIFO | LFU | RANDOM                     (default LRU)
  --policies A,B,...  comma-separated list (overrides --policy)

Other:
  --trace FILE        trace file: one decimal or 0x-hex address per line
  --benchmark         use built-in workloads instead of a trace
  --accesses N        accesses per synthetic workload               (default 200000)
  --seed N            seed for RANDOM policy and workloads          (default 42)
  --repeats N         timing repetitions, median is reported        (default 3)
  --output FILE       also write results as CSV
  --help              show this message
)";

struct Options {
    CacheConfig cfg;
    std::vector<PolicyType> policies{PolicyType::LRU};
    std::string trace;
    std::string output;
    bool benchmark = false;
    bool help = false;
    std::uint64_t seed = 42;
    std::uint64_t accesses = 200000;
    std::uint64_t repeats = 3;
};

// Parses an unsigned integer with an optional K/M/G (x1024) suffix.
std::uint64_t parse_number(const std::string& flag, const std::string& v, bool allow_suffix) {
    if (v.empty() || !std::isdigit(static_cast<unsigned char>(v[0])))
        throw std::invalid_argument(flag + ": expected a non-negative number, got '" + v + "'");
    std::size_t pos = 0;
    unsigned long long x = 0;
    try {
        x = std::stoull(v, &pos);
    } catch (const std::exception&) {
        throw std::invalid_argument(flag + ": number out of range '" + v + "'");
    }
    std::string suffix = v.substr(pos);
    std::uint64_t mult = 1;
    if (!suffix.empty()) {
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(suffix[0])));
        if (!allow_suffix || suffix.size() != 1 || (c != 'K' && c != 'M' && c != 'G'))
            throw std::invalid_argument(flag + ": invalid number '" + v + "'");
        mult = c == 'K' ? (1ull << 10) : c == 'M' ? (1ull << 20) : (1ull << 30);
    }
    if (x > UINT64_MAX / mult) throw std::invalid_argument(flag + ": value too large '" + v + "'");
    return x * mult;
}

std::vector<PolicyType> parse_policy_list(const std::string& csv) {
    std::vector<PolicyType> out;
    std::stringstream ss(csv);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(parse_policy(item));
    if (out.empty()) throw std::invalid_argument("--policies: empty list");
    return out;
}

Options parse_args(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(a + " requires a value");
            return argv[++i];
        };
        if (a == "--help" || a == "-h") o.help = true;
        else if (a == "--benchmark") o.benchmark = true;
        else if (a == "--capacity") o.cfg.capacity_bytes = parse_number(a, value(), true);
        else if (a == "--line-size") o.cfg.line_size = parse_number(a, value(), true);
        else if (a == "--associativity") o.cfg.associativity = parse_number(a, value(), false);
        else if (a == "--policy") o.policies = {parse_policy(value())};
        else if (a == "--policies") o.policies = parse_policy_list(value());
        else if (a == "--trace") o.trace = value();
        else if (a == "--output") o.output = value();
        else if (a == "--seed") o.seed = parse_number(a, value(), false);
        else if (a == "--accesses") o.accesses = parse_number(a, value(), true);
        else if (a == "--repeats") o.repeats = parse_number(a, value(), false);
        else throw std::invalid_argument("unknown option '" + a + "' (see --help)");
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options o = parse_args(argc, argv);
        if (o.help || argc == 1) {
            std::cout << kUsage;
            return 0;
        }
        o.cfg.validate();
        if (o.repeats == 0 || o.repeats > 1000)
            throw std::invalid_argument("--repeats must be between 1 and 1000");

        std::vector<Workload> workloads;
        if (o.benchmark) {
            if (!o.trace.empty()) throw std::invalid_argument("use either --benchmark or --trace");
            if (o.accesses == 0) throw std::invalid_argument("--accesses must be > 0");
            workloads = make_standard_workloads(o.cfg, o.seed, static_cast<std::size_t>(o.accesses));
        } else {
            if (o.trace.empty()) throw std::invalid_argument("provide --trace FILE or --benchmark");
            workloads.push_back({o.trace, read_trace_file(o.trace)});
        }

        std::cout << "cache: " << o.cfg.capacity_bytes << " B, " << o.cfg.line_size
                  << " B lines, " << o.cfg.associativity << "-way, " << o.cfg.num_sets()
                  << " sets, seed " << o.seed << "\n\n";

        const auto results = run_benchmark(workloads, o.cfg, o.policies, o.seed,
                                           static_cast<int>(o.repeats));
        print_results(std::cout, results);

        if (!o.output.empty()) {
            write_csv_file(o.output, results);
            std::cout << "\nwrote " << o.output << '\n';
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
