# Optimization Rules

Optimization priority

1 Correctness

2 Algorithm

3 Cache

4 Memory

5 Parallelism

6 SIMD

7 Assembly

Never optimize before profiling.

Always benchmark.

Never replace GMP multiplication.

Never allocate inside hot loops.

Prefer contiguous memory.

Avoid false sharing.

Use thread affinity when benchmark proves improvement.

Use Huge Pages only if beneficial.

Compression only for Out-of-Core Layer.

Checkpoint should run on dedicated thread.

Worker threads never perform disk IO.

## PR-0019 Binary Splitting Diagnostic

A local four-worker `-O2` diagnostic used an explicit cutoff of 16 terms and
compared exact sequential and staged-parallel P/Q/T results:

| Terms | Sequential | Parallel |
| ---: | ---: | ---: |
| 64 | 79 us | 394 us |
| 256 | 289 us | 156 us |
| 1024 | 1708 us | 814 us |
| 4096 | 10650 us | 4761 us |

This single-host diagnostic demonstrates both the small-range scheduling cost
and the larger-range benefit; it is not a portable production cutoff. The API
therefore requires an explicit cutoff until repeatable target-CPU benchmarks
cover representative digit counts, worker counts, queue capacities, and warm
cache effects. The execution policy also exposes tasks per worker so later CPU,
NUMA, and queue benchmarks can tune task granularity without changing the
Binary Splitting API.
