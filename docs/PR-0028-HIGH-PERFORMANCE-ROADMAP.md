# PR-0028 High-Performance Storage Roadmap

PR-0027 is the correctness and bounded-I/O baseline. PR-0028 may optimize the
implementation only after preserving chunk identity, canonical codec bytes,
NodeLifecycle durability, progress semantics, and the synchronous fallback.

## Phase 0: Measurement contract

- Pin compiler, build type, CPU topology, storage filesystem, and benchmark
  dataset.
- Run warm-cache and cold-cache cases separately.
- Measure split, queue wait, file read/write, CRC32C, codec, and GMP decode as
  independent timings.
- Record throughput, p50/p95 latency, peak RSS, queue depth, and failure count.
- Compare every optimized result with the synchronous P/Q/T reference.

Exit condition: a repeatable baseline exists for 100 MiB, 512 MiB, and 1 GiB
workloads with single- and multi-chunk layouts.

## Phase 1: Concurrent I/O

- Add independent reader and writer worker counts rather than coupling them.
- Benchmark workers 1/2/4/8 and queue capacities 1/4/16/64.
- Separate file I/O from CRC32C and GMP decode where possible.
- Track backpressure wait time and prevent unbounded resident memory.
- Test overlapping read/write workloads and multi-chunk reload ordering.

Exit condition: concurrency improves throughput without changing output,
durability, lifecycle state, or memory-budget guarantees.

## Phase 2: Data movement and codec efficiency

- Reuse bounded buffers and avoid redundant Chunk/P/Q/T copies.
- Evaluate direct I/O, buffered I/O, preallocation, and batched index commits
  only with filesystem-specific measurements.
- Compare `none` and LZ4 by compression ratio, CPU time, store throughput, and
  reload throughput.
- Keep compression selection outside the canonical identity domain.

Exit condition: the selected path wins on the target workload while retaining
corruption detection and restart compatibility.

## Phase 3: Memory and platform placement

- Measure allocator pressure, resident payload lifetime, and peak RSS.
- Evaluate huge pages only for large GMP arenas and only when TLB behavior
  improves in measurements.
- Evaluate NUMA placement, worker affinity, and storage-device locality on the
  target machine.
- Add ASan/UBSan and large-data restart/integrity runs before acceptance.

Exit condition: platform tuning has a measured benefit and no regression in
memory budget, portability, or failure recovery.

## Acceptance gates

- No change to PR-0027 chunk identity or canonical encoded bytes.
- Sync and async output remain bit-for-bit equivalent.
- All existing tests pass under GCC and Clang.
- Large-data sanitizer and restart/integrity tests pass.
- Performance claims include workload, cache state, hardware, compiler, and
  variance information.
