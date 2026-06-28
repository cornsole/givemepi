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