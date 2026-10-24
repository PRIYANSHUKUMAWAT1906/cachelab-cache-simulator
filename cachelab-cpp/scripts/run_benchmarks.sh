#!/usr/bin/env bash
# Builds CacheLab in Release mode and regenerates every CSV in results/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build -j

BIN=./build/cachelab
POLICIES=LRU,FIFO,LFU,RANDOM
ACCESSES=${ACCESSES:-200000}
SEED=${SEED:-42}
REPEATS=${REPEATS:-3}
mkdir -p results

echo "== Main benchmark: 16K, 64B lines, 4-way =="
$BIN --benchmark --capacity 16K --line-size 64 --associativity 4 \
     --policies "$POLICIES" --accesses "$ACCESSES" --seed "$SEED" \
     --repeats "$REPEATS" --output results/benchmark.csv

echo; echo "== Associativity sweep (16K cache) =="
for ways in 1 2 4 8 16; do
  $BIN --benchmark --capacity 16K --associativity "$ways" --policies "$POLICIES" \
       --accesses "$ACCESSES" --seed "$SEED" --repeats "$REPEATS" \
       --output "results/sweep_assoc_${ways}way.csv" > /dev/null
  echo "wrote results/sweep_assoc_${ways}way.csv"
done

echo; echo "== Capacity sweep (4-way) =="
for cap in 4K 8K 16K 32K 64K; do
  $BIN --benchmark --capacity "$cap" --associativity 4 --policies "$POLICIES" \
       --accesses "$ACCESSES" --seed "$SEED" --repeats "$REPEATS" \
       --output "results/sweep_capacity_${cap}.csv" > /dev/null
  echo "wrote results/sweep_capacity_${cap}.csv"
done
