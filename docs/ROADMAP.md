# ROADMAP

Phase 0

[x] Project

[x] CMake

[ ] Logger

[x] Config

[x] CI Automation

----------------------

Phase 1

Platform

- CPUID
- Huge Pages
- NUMA

----------------------

Phase 2

Memory

- Arena
- Pool
- Alignment

----------------------

Phase 3

Scheduler

[x] Thread Pool
[x] Queue (Reference Implementation)
[x] Lock-Free Queue
[x] Work Stealing
[x] Shared Task State (PR-0016)
[x] Task Handle / Future / Join (PR-0016)
[ ] Scheduler Correctness Hardening (PR-0017)
[ ] Drain Shutdown Semantics (PR-0017)
[ ] Global / Local Queue Role Validation (PR-0017)

----------------------

Phase 4

Big Integer

[x] GMP Wrapper

----------------------

Phase 5

Binary Splitting

[x] Node
[x] Merge
[x] Recursive Split
[x] Parallel API Refactoring
[ ] Chudnovsky Leaf Integration (PR-0018)
[ ] Input Range Validation (PR-0018)
[ ] Parallel Cutoff Policy (PR-0019)
[ ] Parallel Execution (PR-0019)
[ ] Parallel Merge (PR-0019)

----------------------

Phase 6

Chudnovsky

[ ] Formula
[ ] Precision Control
[ ] Known-Digits Integration Test

----------------------

Phase 7

Storage

----------------------

Phase 8

Checkpoint

[ ] P/Q/T Block Format (PR-0020)
[ ] Versioned Block Header and Payload Metadata (PR-0020)
[ ] Atomic Temporary File Commit (PR-0020)
[ ] Manifest Format (PR-0020)
[ ] Structural Block Validation (PR-0021)
[ ] Payload Checksum Validation (PR-0021)
[ ] Independent Modular P/Q/T Verification (PR-0021)
[ ] Manifest Consistency Validation (PR-0021)
[ ] Resume from Verified Blocks Only (PR-0021)
[ ] Corrupt Block Quarantine and Recalculation (PR-0021)

----------------------

Phase 9

Progress

[ ] Progress Phase Model (PR-0022)
[ ] Thread-Safe Progress Tracker (PR-0022)
[ ] Immutable Progress Snapshot (PR-0022)
[ ] Terms, Blocks, and Merge-Level Counters (PR-0022)
[ ] Speed, ETA, Memory, and Checkpoint Metrics (PR-0022)
[ ] CLI Text Reporter (PR-0022)
[ ] CLI JSON Reporter (PR-0022)
[ ] TOML and CLI Progress Controls (PR-0022)
[ ] Post-Override Configuration Validation (PR-0022)
[ ] Resume Progress Reconstruction (PR-0022)

----------------------

Phase 10

Verification

[ ] Final Output Hash
[ ] BBP Spot Check
[ ] Known Digits

----------------------

Phase 11

SIMD

----------------------

Phase 12

Assembly

----------------------

Phase 13

Benchmark

----------------------

Phase 14

Performance Tuning
