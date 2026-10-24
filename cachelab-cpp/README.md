# CacheLab — Trace-Driven Cache Replacement Simulator (C++17)

**Author:** Priyanshu Kumawat  
**Email:** [er.priyanshu1906@gmail.com](mailto:er.priyanshu1906@gmail.com)

CacheLab simulates a single-level, set-associative CPU cache and compares replacement
policies (**LRU, FIFO, LFU, Random**) on address traces and built-in synthetic workloads.
It reports hits, misses, evictions, hit/miss rate and simulator run time, and exports CSV.

**Not modelled (by design):** multi-level hierarchies, write-back/write-through, coherence,
prefetching, DRAM timing. Only the hit/miss behaviour of one cache is simulated.

## Build, test, run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

# sanitizer build (GCC/Clang)
cmake -S . -B build-san -DCMAKE_BUILD_TYPE=Debug -DCACHELAB_SANITIZE=ON
cmake --build build-san -j && ctest --test-dir build-san --output-on-failure
```

Requirements: a C++17 compiler and CMake >= 3.14. No external dependencies.

## Browser interface

The React interface can run the built-in workloads or simulate a trace pasted into the text box
or loaded from a file. Choose cache geometry and replacement policies, then run the simulation
and optionally download the results as CSV.

```bash
cd gui
npm ci
npm run dev
```

In a second terminal, start the Gemini API proxy:

```bash
cd gui
# PowerShell
$env:GEMINI_API_KEY = "your-gemini-api-key"
npm run dev:api
```

The browser calls the local Node server; the Gemini key stays on the server and is never
sent to the browser. The API proxy listens on `127.0.0.1:3001`, uses the Google Gen AI SDK's
Interactions API with `gemini-3.8-flash`, and does not store interactions. If Gemini returns
HTTP 503 for temporary overload, the server retries once with `gemini-3.6-flash`. The Vite
development server forwards `/api` requests to the Node proxy.

To create a production build, run `npm ci` followed by `npm run build` from `gui`; the generated
files are written to `gui/dist` and are intentionally not included in the source tree.
The browser interface implements the same cache mapping, policy behavior, workloads, and seeded
MT19937-64 random generator as the C++ app. The browser UI limits cache size and benchmark length
to keep runs responsive.

```bash
# simulate a trace with every policy
./build/cachelab --capacity 16K --line-size 64 --associativity 4 \
    --policies LRU,FIFO,LFU,RANDOM --trace traces/sample.trace --output results/sample.csv

# built-in synthetic workloads
./build/cachelab --benchmark --policies LRU,FIFO,LFU,RANDOM --output results/benchmark.csv

# regenerate everything in results/ (main benchmark + associativity and capacity sweeps)
./scripts/run_benchmarks.sh
```

Options: `--capacity`, `--line-size`, `--associativity`, `--policy`, `--policies`, `--trace`,
`--benchmark`, `--accesses`, `--seed`, `--repeats`, `--output`, `--help`.
Capacity/line size accept `K`/`M`/`G` suffixes. Trace files hold one decimal or `0x` hex byte
address per line; blank lines and `#` comments are ignored. Malformed input (bad digits, negatives,
extra tokens, >64-bit values) is rejected with `file:line: message`.

## Design

```
main.cpp  ->  TraceReader / make_standard_workloads  ->  CacheSimulator  ->  Statistics
                                                              |
                                                     ReplacementPolicy (LRU | FIFO | LFU | Random)
benchmark.cpp: runs workloads x policies, times them, writes CSV
```

Address mapping (done once, in `CacheSimulator`, shared by all policies):

```
block = address / line_size      set = block % num_sets      tag = block / num_sets
num_sets = capacity / (line_size * associativity)
```

On a miss the simulator fills an invalid way first (never counted as an eviction); only when the
set is full does it call `ReplacementPolicy::select_victim`. The policy interface is three events
(`on_hit`, `on_insert`, `select_victim`); each policy owns its own metadata.

| Policy | Metadata | Victim |
|--------|----------|--------|
| LRU    | last-use timestamp per way | smallest timestamp; hits refresh it |
| FIFO   | insertion timestamp per way | smallest timestamp; hits do **not** refresh it |
| LFU    | access count + insertion timestamp | lowest count, ties -> oldest inserted; count restarts at 1 on insert |
| Random | seeded `mt19937_64` | unbiased bounded sampling via rejection (portable, reproducible for a given seed) |

The browser uses the same rejection-sampling algorithm and seeded generator as the C++ app.

Victim selection is O(ways) per miss, which is optimal in practice for small associativity.

## Workloads (`--benchmark`)

All are deterministic for a given `--seed` and sized relative to the cache geometry.

| Name | Pattern |
|------|---------|
| `sequential`   | 8-byte-stride scan (only spatial locality: 7 of 8 accesses share a 64 B line) |
| `loop_fits`    | cyclic reuse of 50% of the cache's lines |
| `loop_exceeds` | cyclic reuse of 125% of the cache's lines |
| `random`       | uniform over 8x the cache capacity |
| `hotset`       | 80% of accesses to a hot set (1/8 of the cache's lines), 20% to a 16x-cache cold range |
| `conflict`     | cycles through `ways+1` blocks that all map to set 0 |

## Results

Regenerated from the current source with the benchmark arguments used by
`./scripts/run_benchmarks.sh`: 200,000 accesses per workload, seed 42, and three timed repetitions.
The experiment used Windows x64, MSVC 19.44.35229.0 (Visual Studio 2022), CMake 4.4.4, and a
Release build on 2026-10-24. Each workload/policy run first performs one discarded warm-up, then
reports the median of the three measured runs; hit/miss statistics are collected in a separate
untimed pass.
Hit rates (%) are deterministic. **`time_us` depends on the machine and is not reproducible** —
treat it as a rough relative figure only.

**16 KiB, 64 B lines, 4-way (64 sets)** — `results/benchmark.csv`

| Workload     | LRU   | FIFO  | LFU   | Random |
|--------------|------:|------:|------:|-------:|
| sequential   | 87.50 | 87.50 | 87.50 | 87.50 |
| loop_fits    | 99.94 | 99.94 | 99.94 | 99.94 |
| loop_exceeds |  0.00 |  0.00 |  0.00 | 59.94 |
| random       | 12.51 | 12.49 | 12.45 | 12.56 |
| hotset       | 81.08 | 78.80 | 81.10 | 78.80 |
| conflict     |  0.00 |  0.00 |  0.00 | 60.00 |

**Associativity sweep, 16 KiB cache** (`results/sweep_assoc_*.csv`)

| Workload / policy       | 1-way | 2-way | 4-way | 8-way | 16-way |
|-------------------------|------:|------:|------:|------:|-------:|
| loop_exceeds / LRU      | 59.90 | 39.94 |  0.00 |  0.00 |   0.00 |
| loop_exceeds / Random   | 59.90 | 59.89 | 59.94 | 61.49 |  62.12 |
| conflict / LRU          |  0.00 |  0.00 |  0.00 |  0.00 |   0.00 |
| conflict / Random       |  0.00 | 33.28 | 60.00 | 77.72 |  88.20 |
| hotset / LRU            | 78.61 | 80.80 | 81.08 | 81.08 |  81.09 |
| hotset / FIFO           | 78.61 | 78.75 | 78.80 | 78.79 |  78.66 |

The browser and C++ implementations were also compared using the same 6,000-address trace across
four cache geometries, three seeds (including the maximum 64-bit seed), and all four policies.
Access, hit, miss, and eviction counts matched exactly in all 48 comparisons.

### What the numbers show

- **LRU/FIFO/LFU collapse on cyclic patterns larger than the cache.** In `loop_exceeds` (4-way+) and
  `conflict`, the block about to be reused is always the one just evicted, so they score 0%. Random
  breaks the cycle and keeps ~60%. This is the classic LRU pathology, not a bug: the tests verify
  LRU against an independent reference model.
- **Direct-mapped caches (1-way) make every policy identical** — there is no victim choice. The sweep
  confirms this (all four columns equal at 1-way).
- **Recency/frequency help on skewed workloads.** On `hotset`, LRU and LFU (~81.1%) beat FIFO and Random
  (~78.8%) because they keep hot blocks resident, while FIFO eventually evicts hot blocks by age alone.
- **No policy helps `random`** (~12.5%): with no reuse pattern the hit rate is just the fraction of the
  footprint that fits (1/8).
- **Caveat:** `loop_exceeds` and `hotset` are sized relative to capacity, so their hit rates are flat in
  the capacity sweep (`results/sweep_capacity_*.csv`); only the associativity sweep is informative for them.

## Testing

`ctest` runs four dependency-free suites (249 checks total):
cache mapping and config validation, policy behaviour (hand-computed LRU/FIFO/LFU sequences,
LRU vs a list-based reference model on 20,000 random accesses, Random reproducibility),
trace parsing (valid/invalid/empty/overflow/line numbers), and workloads/CSV.
They also pass under ASan + UBSan. A mutation check (disabling LRU's recency update) makes the
policy suite fail, confirming the tests detect real regressions.

## Limitations / future work

Single-level cache; reads only; no write policies, prefetching or timing model; LFU uses per-line
counters (no aging); trace format is plain addresses. Natural extensions: pseudo-LRU, SRRIP/DRRIP
(RRIP policies from ChampSim), Belady's optimal as an upper bound, multi-level hierarchy.
