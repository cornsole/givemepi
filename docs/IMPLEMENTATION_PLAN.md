# IMPLEMENTATION PLAN

## Purpose

This document contains approved implementation plans and future PR boundaries.

It is maintained separately from `PR-progress/`, which is reserved for the
developer's own work records.

Agent-authored plans are added or changed here only after user approval.

## Approved Execution Order

| PR | Goal | Status |
| --- | --- | --- |
| PR-0016 | Task synchronization foundation | Complete |
| PR-0017 | Scheduler correctness hardening | Complete |
| PR-0018 | Chudnovsky leaf and range validation | Complete |
| PR-0019 | Cutoff-based parallel Binary Splitting | Complete |
| PR-0020 | End-to-end Chudnovsky calculation | Complete |
| PR-0021 | P/Q/T checkpoint block foundation | Complete |
| PR-0022 | Checkpoint integrity verification | Complete |
| PR-0023 | Progress snapshot and reporting | Complete |
| PR-0024 | Final output verification | Complete |
| PR-0025 | Runtime storage foundation | Complete |
| PR-0026 | Synchronous out-of-core stabilization | Complete |
| PR-0027 | Asynchronous storage pipeline | Complete |
| PR-0028 | Concurrent storage pipeline implementation | Complete |
| PR-0029 | Measurement and bottleneck optimization | Complete |
| PR-0030 | Durable checkpoint emission and verified resume | Complete |
| PR-0031 | Live checkpoint telemetry and CLI resume regression | Complete |
| PR-0032 | Default CLI progress and checkpoint hardening | Complete |
| PR-0033 | CLI help and usage errors | Complete |
| PR-0034 | Logger foundation and resume diagnostics | Complete |
| PR-0035 | Informational benchmark workflow | Complete |
| PR-0036 | Tag-gated source release workflow | Complete |
| PR-0037 | End-to-end and current-phase ETA | Complete |
| PR-0038 | Evidence-driven total and phase ETA | Complete |
| PR-0039 | Configurable text ETA units and precision | Complete |
| PR-0040 | Runtime storage run isolation | Complete |
| PR-0041 | Cross-precision checkpoint promotion | Complete |
| PR-0042 | Computation memory budget and fail-fast admission | Approved; planned after PR-0041 |

---

## PR-0016: Task Synchronization Foundation

### Goal

Add a shared completion state between `Task` and `TaskHandle` so callers can
wait for submitted work, observe completion or failure, and safely join
scheduler tasks.

This PR establishes synchronization only. Scheduler queue routing, drain
shutdown semantics, and Binary Splitting parallel execution remain separate
follow-up work.

### Architecture Contract

`Task` and its returned `TaskHandle` share one `TaskSharedState` through
`std::shared_ptr`.

The shared state owns:

- `TaskState` (`Pending`, `Running`, `Completed`, `Failed`)
- `std::exception_ptr`
- completion mutex
- completion condition variable

`TaskHandle` does not own or reference the `Task` object itself. It remains a
move-only observation handle over the shared completion state.

Task exceptions are captured in the shared state and must not escape the
worker thread. Callers inspect or rethrow failures through `TaskHandle`.

### Work Sequence

1. Define the shared task state.
2. Update `TaskHandle` to observe the shared state.
3. Update `Task` to publish state changes and captured failures.
4. Align `ThreadPool::submit()` and `Scheduler::submit()` on `TaskHandle`.
5. Return invalid handles for rejected submissions.
6. Preserve the existing queue and worker topology.
7. Add wait, completion, failure, rejection, and multi-handle join tests.
8. Update CMake if an out-of-line `TaskHandle.cpp` is added.
9. Build all targets, run CTest, and run `git diff --check`.
10. Update completion documentation.

### Files In Scope

- `include/scheduler/Task.hpp`
- `include/scheduler/TaskHandle.hpp`
- `src/scheduler/Task.cpp`
- `src/scheduler/TaskHandle.cpp` if required
- `include/scheduler/ThreadPool.hpp`
- `src/scheduler/ThreadPool.cpp`
- `include/scheduler/Scheduler.hpp`
- `src/scheduler/Scheduler.cpp`
- `tests/scheduler/SchedulerTest.cpp`
- `tests/scheduler/ThreadPoolTest.cpp`
- `CMakeLists.txt`
- scheduler documentation

### Tests

- valid handle after accepted submission
- invalid handle after rejected submission
- `wait()` blocks until completion
- completed state is observable
- failed state and captured exception are observable
- multiple handles can be joined without polling loops
- submission before start and after stop is rejected
- zero-worker submission is rejected

### Out of Scope

- LockFreeQueue redesign
- global/local queue routing changes
- drain or cancellation shutdown modes
- worker idle-wait optimization
- Binary Splitting parallel execution
- Chudnovsky leaf implementation
- checkpoint implementation
- custom FFT or NTT multiplication
- NUMA and Huge Page integration
- repository-wide formatting changes

### Definition of Done

- Project builds successfully.
- All existing and new synchronization tests pass.
- No task exception escapes a worker thread.
- Accepted tasks return valid handles.
- Rejected tasks return invalid handles.
- Callers can wait for multiple tasks without polling.
- Documentation matches the implemented behavior.

---

## PR-0017: Scheduler Correctness Hardening

### Goal

Stabilize scheduler lifecycle and queue semantics before parallel Binary
Splitting depends on them.

### Scope

- define drain shutdown semantics for accepted work
- route external submissions through the global queue
- route worker-created tasks through local queues
- make queue capacity and submission rejection observable
- add direct concurrent LockFreeQueue tests
- add observable work-stealing behavior tests
- preserve `ThreadPool` ownership of workers and the `IQueue` abstraction

### Lifecycle Contract

The scheduler exposes `Stopped`, `Running`, and `Stopping` states.

- A newly constructed scheduler is `Stopped`.
- `start()` transitions `Stopped` to `Running`.
- Calling `start()` while already `Running` is a no-op.
- `stop()` transitions `Running` through `Stopping` to `Stopped`.
- Calling `stop()` while already `Stopped` is a no-op.
- A concurrent `stop()` waits for the active stop generation to finish.
- A concurrent `start()` waits for `Stopping` to finish and then restarts.
- Restart after a completed stop is supported.
- Submission is accepted only in `Running`; `Stopped` and `Stopping` reject it.
- `running()` is true only in `Running`.

The lifecycle mutex is not held while worker threads are joined, preventing a
worker task that attempts submission during shutdown from deadlocking the stop
path. Work accepted before `Stopping` follows the drain contract below.

### Drain Shutdown Contract

`ThreadPool` counts every accepted task until a worker finishes executing it.
The count includes queued and currently executing work in both global and local
queues.

- The transition to `Stopping` rejects every new submission.
- Stop requests are published to every worker before any worker is joined.
- Workers continue local pop, global pop, and steal attempts while accepted
  work remains.
- A worker exits only after stop was requested and the accepted outstanding
  task count reaches zero.
- Failed tasks count as finished after their failure state is published.
- Queue insertion rejection rolls back its outstanding-task reservation.
- `stop()` returns only after all accepted handles are terminal and all workers
  are joined.
- A restarted pool begins with no outstanding work from the previous run.

The outstanding count is the authoritative drain condition. A transient
queue-empty observation is not sufficient because another worker may still be
executing an accepted task.

### Global and Local Queue Contract

- Calls from threads outside this pool publish to the bounded global `IQueue`.
- Calls from a worker owned by this pool publish to that worker's local queue.
- Calls from a worker owned by another pool are external submissions and use
  this pool's global queue.
- A full global queue returns an invalid `TaskHandle`.
- Local worker submission retains the same public `submit(Task)` API.
- Worker context is active only while the worker execution loop is running.

### Lock-Free Queue Correctness Contract

`LockFreeQueue` is a bounded multi-producer, multi-consumer queue. Its capacity
must be between two and `PTRDIFF_MAX`, inclusive. Capacity zero cannot index a
slot, and capacity one cannot distinguish the occupied and reusable sequence
states required by this queue protocol.

Each slot sequence is compared with the producer or consumer's expected
sequence:

- an equal sequence allows that operation to claim the position
- a sequence behind the expectation means full for a producer or empty for a
  consumer
- a sequence ahead of the expectation means the observed position is stale,
  so the operation reloads its shared position and retries

Queue positions are coordination counters and use relaxed atomic operations.
The per-slot sequence uses release when publishing a task or releasing a
consumed slot and acquire before reading that slot. This sequence edge protects
the non-atomic `Task` payload.

`empty()` examines the next consumer slot rather than comparing reservation
counters. It is a concurrent snapshot only and is never used as the drain
shutdown invariant; accepted outstanding-task accounting remains authoritative
for shutdown.

Direct tests cover invalid capacity, bounded FIFO behavior, repeated slot
reuse, concurrent producers with unused capacity, and concurrent producers and
consumers with exactly-once task execution.

### Direct Concurrency Test Contract

Scheduler concurrency tests use explicit gates and observable lifecycle states
instead of timing sleeps.

- All workers are first held inside accepted blocker tasks so the global queue
  and the `Stopping` state remain stable while producers race.
- Multiple external producers fill the queue while `Running`; every returned
  valid handle must become terminal and its task must execute exactly once.
- After `Stopping` is observed, the same producers submit another batch; every
  submission must return an invalid handle and none of those callables may run.
- Releasing the blockers allows shutdown to drain the accepted batch before
  returning.
- Multiple callers invoke `start()` and `stop()` together over repeated cycles;
  every cycle must expose one stable lifecycle state and complete all accepted
  handles before the stop callers return.

LockFreeQueue MPMC tests and scheduler lifecycle/drain tests remain separate
CTest targets so failures identify whether the queue algorithm or scheduler
integration violated its contract.

### Observable Work-Stealing Test Contract

Load completion and actual steal behavior use separate CTest targets.

- `WorkStealingTest` remains the high-volume scheduler completion regression.
- `WorkStealingBehaviorTest` supplies a test queue that accepts exactly one
  global root task and records every global push attempt.
- The root submits child tasks through the public scheduler API, placing them
  in its worker-local queue, and then blocks before it can pop that queue.
- Child tasks block when entered so at least two other worker threads must steal
  from the root's local queue before the test releases them.
- No child execution thread may equal the blocked root thread.
- The global queue must observe only the root push, proving child execution did
  not use a global-queue fallback.
- Every child handle must complete and every child callable must execute exactly
  once.

The test uses condition variables and thread identity observations rather than
sleep-based timing assumptions.

### Deferred Optimization

Worker idle-wait and wake-up optimization remains benchmark-driven follow-up
work unless required for scheduler correctness.

### Completion

PR-0017 was completed on 2026-07-20.

- Scheduler lifecycle, drain shutdown, and queue-routing contracts are
  implemented and documented.
- LockFreeQueue boundary and contention correctness have direct MPMC coverage.
- Scheduler concurrency and observable work-stealing behavior have dedicated
  CTest targets.
- GCC ASan/UBSan and Clang builds pass the full test suite.
- Concurrency and work-stealing tests pass more than 100 repeated runs.

### Next Contributor TODO

Begin PR-0018 with Chudnovsky leaf and `[start, end)` range correctness only.
Preserve the scheduler contracts established here, keep Binary Splitting
parallel execution deferred to PR-0019, and add known leaf and small-range
P/Q/T expectations before changing the parallel path.

---

## PR-0018: Chudnovsky Leaf and Range Validation

### Goal

Turn sequential Binary Splitting from a structural placeholder into a correct
Chudnovsky P/Q/T computation.

### Binary Splitting Contract

Every Binary Splitting range uses the half-open interval `[start, end)`.
Public split entry points reject empty or reversed ranges where `start >= end`
before performing unsigned range arithmetic or entering recursion.

For the single-term range `[k, k + 1)`, the leaf values are:

- when `k == 0`, `P = 1`, `Q = 1`, and `T = 13591409`
- when `k > 0`, `P = (6k - 5)(2k - 1)(6k - 1)`
- when `k > 0`, `Q = k^3 * 10939058860032000`
- when `k > 0`, `T = P * (13591409 + 545140134k)`
- `T` is negated when `k` is odd

Leaf coefficient arithmetic is performed with `GMPInteger` after converting
the term index. Intermediate coefficient products must not be evaluated in a
fixed-width integer type where they can overflow before reaching GMP.

Adjacent nodes `[a, m)` and `[m, b)` merge into `[a, b)` using:

- `P = P_left * P_right`
- `Q = Q_left * Q_right`
- `T = T_left * Q_right + P_left * T_right`

`split()` remains the default sequential entry point and delegates to
`splitSequential()`. `splitParallel()` must return the same mathematical result
and enforce the same range contract, but scheduler-backed execution, cutoff
selection, and parallel merge remain deferred to PR-0019.

### Scope

- validate `[start, end)` input ranges
- implement Chudnovsky leaf P/Q/T values
- test known leaf and small-range P/Q/T results
- add a small known-digits integration test
- preserve the existing merge equation and sequential API

### Completion

PR-0018 was completed on 2026-07-20.

- Sequential Binary Splitting now produces Chudnovsky P/Q/T leaf values.
- Every public split entry point rejects empty and reversed ranges.
- Leaf, small-range, upper-boundary, and multi-level recursive results have
  direct regression coverage.
- A test-only GMP calculation verifies 20 decimal places of pi from `[0, 3)`.
- GCC, Clang, ASan, and UBSan pass the full test suite; LeakSanitizer is not
  runnable under the workspace ptrace restrictions.

### Next Contributor TODO

Begin PR-0019 by defining and measuring a sequential cutoff before introducing
scheduler submissions. Preserve exact equality with `splitSequential()`, join
every submitted child through `TaskHandle`, and retain a sequential fallback
for small ranges and rejected submissions.

---

## PR-0019: Cutoff-Based Parallel Binary Splitting

### Goal

Use the stabilized scheduler to execute large Binary Splitting ranges in
parallel while keeping small ranges sequential.

### Scope

- define a configurable or measured sequential cutoff
- submit only sufficiently large child ranges
- join child `TaskHandle` values before merge
- preserve sequential fallback
- test sequential and parallel result equality with real Chudnovsky values
- measure scheduling overhead before tuning the cutoff

### Parallel Execution Contract

PR-0019 uses a staged Binary Splitting DAG instead of recursive worker tasks
that submit children and block. The current scheduler does not execute queued
work while a worker waits on a `TaskHandle`; recursive fork/join could therefore
leave every worker waiting while child tasks remain queued.

The caller partitions `[start, end)` into balanced sequential leaf blocks. The
number of blocks is bounded by the worker count so task metadata and scheduler
overhead do not grow linearly with a very large term count. Each accepted task
writes one exclusively owned result slot and never waits for another task.

After all leaf blocks are complete, adjacent results are merged in parallel
levels until one root remains. The caller joins each complete level before
starting the next, making dependencies explicit and providing natural future
boundaries for checkpointing, progress reporting, NUMA placement, and memory
reclamation.

The public parallel API receives a `Scheduler&` and an explicit execution
policy containing the sequential cutoff and target tasks per worker. A
cutoff-only convenience overload retains a measured default task multiplier.
It does not use a global scheduler or read application configuration.
If the range is below the cutoff, the scheduler is stopped or has no workers,
the call originates from one of the scheduler's own workers, or an individual
submission is rejected, affected work executes synchronously. Every path must
produce exactly the same P/Q/T result as `splitSequential()`.

Task failures are rethrown by the caller after joining their handles. Result
storage remains internal to Binary Splitting; PR-0019 does not turn the
scheduler completion-only `TaskHandle` into a generic future.

### Work Sequence

1. Expose read-only detection of the scheduler's current worker context.
2. Add the scheduler-aware `splitParallel()` overload and validate cutoff.
3. Partition large ranges into a bounded set of balanced sequential blocks.
4. Execute leaf blocks through accepted tasks or synchronous fallback.
5. Join and rethrow task failures before consuming result slots.
6. Execute adjacent merge pairs as staged parallel levels.
7. Test exact equality, cutoff behavior, stopped/zero-worker fallback,
   submission rejection, worker-call fallback, and observable concurrency.
8. Measure representative task sizes before selecting a production default.

### Completion

PR-0019 was completed on 2026-07-20.

- Large ranges execute as bounded sequential leaf blocks followed by staged
  parallel merge levels.
- Tasks never wait for child tasks, avoiding worker-starvation deadlocks.
- Stopped, zero-worker, owned-worker, and rejected-submission paths fall back
  synchronously without changing P/Q/T results.
- Tests observe task acquisition by multiple worker threads and verify exact
  sequential equality across cutoff and uneven-range cases.
- The task count is bounded by the configurable tasks-per-worker policy and the
  final merge levels execute inline when no parallel pair work remains.
- A diagnostic four-worker `-O2` measurement showed scheduler overhead at 64
  terms and useful speedup by 256 terms; the cutoff remains explicit until the
  production workload and target CPUs have a benchmark suite.

### Next Contributor TODO

Begin PR-0020 with the minimum end-to-end Chudnovsky calculation path. Convert
requested digits to a guarded term count, execute the existing Binary
Splitting engine, finalize pi with integer fixed-point arithmetic, and verify
known decimal output before defining checkpoint computation identity.

---

## PR-0020: End-to-End Chudnovsky Calculation

### Goal

Turn the verified P/Q/T engine into a minimal production calculation path from
requested decimal digits to a normalized pi string.

### Arithmetic Contract

The finalization path remains integer based. For requested digits `d`, compute
with explicit guard digits, derive the required Chudnovsky term count, and use
the existing sequential or staged-parallel Binary Splitting engine.

The square-root factor is produced with GMP integer square root at the guarded
decimal scale. Final division, guard removal, and decimal formatting operate on
integers so correctness does not depend on host floating-point behavior or an
implicit global GMP floating-point precision.

### Scope

- define requested-digit, guard-digit, and term-count policy
- add required GMPInteger power-of-ten, square-root, and division operations
- implement `426880 * sqrt(10005) * Q / T` at guarded fixed-point precision
- normalize output as `3.<requested digits>`
- expose a computation API independent from CLI, checkpoint, and reporting
- support sequential and scheduler-backed Binary Splitting execution
- test known digits across multiple requested precisions
- add an end-to-end benchmark separating split, finalize, and format time

### Out of Scope

- checkpoint files and resume
- progress reporters
- BBP verification and final output hashing
- automatic CPU topology and cutoff tuning
- out-of-core decimal conversion

### Definition of Done

- requested digits deterministically map to sufficient guarded terms
- sequential and parallel calculation produce identical normalized output
- known pi prefixes pass at multiple precision boundaries
- invalid digit, guard, and scheduler policy inputs are rejected
- end-to-end benchmark results are recorded before further optimization
- the resulting computation identity fields are documented for PR-0021

### Completion

PR-0020 was completed on 2026-07-20.

- Requested digits map deterministically to guarded working digits, a
  conservative term count, and estimated binary precision without
  floating-point policy arithmetic.
- Sequential and staged-parallel calculation return identical normalized,
  half-up-rounded decimal strings with their immutable precision identity.
- Exact known rounded values pass from 1 through 100 decimal places, and 1,000
  digits remain identical across 16, 32, and 64 guard digits.
- GCC, Clang, ASan, UBSan, and analyzer checks pass the full suite.
- An opt-in Release benchmark records split, integer finalization, formatting,
  and total time through one million digits while validating full output.

### Computation Identity for PR-0021

Checkpoint identity must include requested digits, guard digits, working
digits, term count, arithmetic-format version, Binary Splitting range, and
parallel policy where it affects block boundaries. Runtime timing values are
observations and are not part of computation identity.

### Next Contributor TODO

Begin PR-0021 with a versioned P/Q/T checkpoint block format derived from this
computation identity. Keep worker threads free of disk I/O and preserve the
staged leaf-block boundary as the initial checkpoint granularity.

---

## PR-0021: P/Q/T Checkpoint Block Foundation

### Goal

Define a durable, versioned checkpoint block for one completed Binary Splitting
range without implementing resume policy yet.

### Scope

- versioned block header with file magic
- target computation identity
- range `[a, b)` and tree level
- P/Q/T payload lengths
- checksum type and checksum value fields
- GMP integer serialization and deserialization
- dedicated checkpoint writer path
- temporary-file write, flush, synchronization, and atomic rename
- versioned manifest format
- round-trip and interrupted-write tests

### Computation Identity Contract

Checkpoint compatibility is based on mathematical identity, not execution
policy. The identity contains:

- algorithm identifier and arithmetic-format version
- requested digits and guard digits
- working digits and Chudnovsky term count
- Binary Splitting formula/version identifier

Worker count, cutoff, tasks per worker, timing values, and queue capacity do not
change P/Q/T and therefore are not compatibility fields. They may be recorded
as optional provenance. Each block's `[start, end)` and tree level define its
position independently of the parallel partition policy that produced it.

### Canonical Block Format

The codec writes fields explicitly and never dumps a native C++ struct. All
fixed-width integers use big-endian byte order. Version 1 contains:

1. fixed magic and format version
2. encoded header size and flags
3. mathematical computation identity
4. `[start, end)` and tree level
5. canonical P/Q/T sign and magnitude lengths
6. checksum algorithm, checksum length, and checksum value
7. canonical P/Q/T payloads

GMP values use signed magnitude encoding. Zero has sign `zero` and an empty
magnitude; positive and negative values use a minimal unsigned big-endian
magnitude with no leading zero bytes. The decoder rejects non-canonical values
instead of silently normalizing them.

The initial checksum algorithm is CRC32C. It covers the canonical header with
the checksum bytes zeroed plus the complete P/Q/T payload. CRC32C protects
against accidental storage corruption; it does not replace PR-0022 independent
modular P/Q/T verification.

### Atomic Commit Contract

Block and manifest updates use a temporary file in the destination directory:

1. encode the complete content and checksum
2. create a unique temporary file without replacing another writer's temp file
3. write every byte and handle short writes
4. flush and synchronize the file
5. close the file successfully
6. atomically rename it to the deterministic final path
7. synchronize the parent directory

No final filename is visible before the full file is durable. A failed write,
sync, close, or rename leaves the previous final file untouched and removes or
leaves only an ignorable temporary file. Tests use injected commit fault points
rather than process timing races.

### Manifest Contract

The versioned manifest is an index, not proof that a block is reusable. It
contains the computation identity and deterministic entries with range, level,
filename, payload size, checksum metadata, and completion state. Entries are
sorted by range and level before encoding so identical state produces identical
bytes. Manifest replacement uses the same atomic commit protocol as blocks.

PR-0022 remains responsible for validating block structure, checksums,
manifest gaps/overlaps, and modular P/Q/T residues before resume trusts an
entry.

### Work Sequence

1. Define version constants, limits, computation identity, block metadata, and
   manifest entry value types without filesystem behavior.
2. Add canonical GMP signed-magnitude export/import and boundary tests for
   zero, positive, negative, and very large values.
3. Implement endian-safe primitive encoding/decoding with checked size
   arithmetic and no native struct serialization.
4. Implement CRC32C and fixed checksum-coverage fixtures.
5. Encode/decode an in-memory version-1 P/Q/T block and verify byte-stable
   round trips.
6. Implement same-directory temporary write, file sync, atomic rename, and
   parent-directory sync behind one storage component.
7. Add deterministic block naming and a synchronous `CheckpointStore` called
   only outside scheduler worker tasks.
8. Implement deterministic manifest encoding and atomic replacement.
9. Add fault-injected interrupted-write tests proving the old final file is
   preserved and partial files are never accepted as final.
10. Run GCC, Clang, ASan, UBSan, analyzer, filesystem round-trip, and repeated
    atomic-commit tests before documenting completion.

### Boundary

Worker threads never perform checkpoint disk I/O. PR-0021 provides a
synchronous checkpoint storage path that the stage coordinator may call only
after completed nodes have been joined. A dedicated asynchronous writer thread
may wrap this path later without changing the block codec or commit protocol.

PR-0021 writes and round-trips checkpoint data but does not implement resume,
trust decisions, quarantine, corruption recovery, compression, or worker-stage
integration.

The presence of a block file does not make it reusable. PR-0022 performs all
validation before resume accepts it.

### Definition of Done

- A completed P/Q/T range round-trips without value loss.
- Partial files never receive a final checkpoint name.
- The header can be extended through format versioning.
- Checksum metadata is part of the initial format.
- Existing computation paths remain independent from storage details.
- Canonical bytes are independent of host endianness and struct layout.
- Failed atomic commits cannot replace an existing valid final file.
- Equivalent manifests encode to identical bytes regardless of insertion order.

### Completion

PR-0021 was completed on 2026-07-20.

- Added portable computation identity, block location, and execution provenance
  value types with mathematical compatibility validation.
- Added canonical signed-magnitude GMP serialization and a versioned,
  endian-independent P/Q/T block codec protected by CRC32C.
- Added synchronous deterministic checkpoint storage with same-directory temp
  files, file and directory synchronization, and atomic replacement.
- Added a canonical versioned manifest with deterministic ordering, CRC32C,
  and atomic replacement.
- Verified byte-stable memory and filesystem round trips, corruption rejection,
  pre-rename fault preservation, GCC, Clang, ASan, UBSan, and static analysis.

---

## PR-0022: Checkpoint Integrity Verification

### Goal

Allow resume to trust only structurally valid, storage-valid, and
mathematically verified P/Q/T blocks.

### Validation Pipeline

1. Validate file magic and supported format version.
2. Validate computation identity, range, and tree level.
3. Validate P/Q/T payload boundaries and lengths.
4. Validate payload checksum.
5. Validate manifest membership, overlap, gaps, and completion state.
6. Independently recompute P/Q/T residues modulo selected verification primes.
7. Accept the block or quarantine it and reschedule its range.

### Resume Rules

- Ignore incomplete temporary files.
- Never merge an unvalidated block.
- Recalculate missing, corrupt, incompatible, or mathematically invalid ranges.
- Reconstruct progress from accepted blocks instead of a saved percentage.
- Preserve format-version compatibility rules in one validation component.

### Tests

- truncated header and payload
- unsupported version
- mismatched target identity
- invalid range or tree level
- checksum mismatch
- manifest overlap and gap
- valid checksum with invalid modular P/Q/T result
- interrupted temporary file
- mixed valid and invalid blocks during resume

### Definition of Done

- Resume schedules only ranges not covered by validated blocks.
- Invalid data cannot enter the merge tree.
- Every rejection has a diagnostic reason.
- Validation is independent from CLI presentation.

---

## PR-0023: Progress Snapshot and Reporting

### Goal

Expose reliable computation progress to the CLI while preserving an extension
boundary for JSON, status files, GUI, metrics, and remote monitoring.

### Architecture

```text
Computation / Scheduler / Checkpoint
                |
                v
        ProgressTracker
                |
                v
       ProgressSnapshot
                |
       +--------+--------+
       |                 |
       v                 v
CLI Text Reporter   CLI JSON Reporter
                          |
                          v
                 Future Reporter Types
```

### Snapshot Fields

- schema version
- computation phase
- target digits
- total and completed Chudnovsky terms
- total and completed checkpoint blocks
- current merge level
- active and queued task counts
- elapsed time
- throughput
- optional ETA
- memory use
- checkpoint bytes
- last validated checkpoint
- terminal completion or failure state

Raw counters are authoritative. Percentage, throughput, and ETA are derived
values and may be unavailable until enough samples exist.

### Configuration and CLI

- TOML: `progress`, `progress_interval`, `progress_format`
- CLI: `--progress`, `--no-progress`
- CLI: `--progress-interval <milliseconds>`
- CLI: `--progress-format text|json`
- run shared configuration validation after command-line overrides

### Reporting Rules

- computation workers never format or print progress
- reporting runs on a dedicated path
- TTY text may update one line
- non-TTY text emits complete timestamped records
- JSON output uses a versioned machine-readable schema
- slow reporters cannot block computation workers
- resume reconstructs counters from PR-0022 validated blocks

### Tests

- monotonic raw progress counters
- stable immutable snapshots under concurrent updates
- phase transitions
- unknown ETA before sufficient samples
- text and JSON reporter output
- TTY and non-TTY behavior
- progress disabled mode
- interval and format validation
- resume reconstruction from validated block metadata
- slow reporter isolation

### Definition of Done

- CLI users can continuously see phase and completed work.
- Automation can consume versioned JSON progress.
- No algorithm or scheduler component depends on CLI formatting.
- Future reporter types require no changes to computation code.

---

## PR-0024: Final Output Verification

### Goal

Publish a final decimal result only after independent structural, cryptographic,
and mathematical verification, while supporting bounded-memory stored-file
inspection and durable external verification evidence.

### Implemented Architecture

```text
canonical output inspection
        |
        +--> versioned known digits
        +--> streaming SHA-256
        +--> GMP decimal-to-hex extraction
                         |
                         +--> deterministic BBP samples
        |
        v
FinalVerificationReport
        |
        v
versioned CRC32C verification manifest
```

### Contracts

- memory output is exactly `3.` plus requested ASCII digits;
- stored output adds exactly one LF and no other trailing bytes;
- SHA-256 excludes the LF;
- known digits are rounded at the requested precision;
- BBP positions are zero-based hexadecimal fractional positions;
- decimal-to-hex uses exact GMP integer arithmetic and a half-ULP interval;
- unresolved numeric boundaries are inconclusive, never guessed;
- malformed structure stops mathematical verification;
- known-digit failure does not suppress diagnostic hashing or BBP checks;
- passed manifests use canonical version-1 bytes, CRC32C, and atomic storage;
- manifest hash revalidation detects arbitrary stored-output mutation;
- CLI progress flows through writing, verifying, then terminal completion.

### Validation

- normal 1, 10, 50, 100, and 1,000-digit calculations;
- malformed structure, known-prefix mutation, BBP mismatch and inconclusive;
- middle and final file-digit mutation against manifest SHA-256;
- SHA-256 and known hexadecimal standard references;
- canonical manifest round-trip, corruption, and atomic failure injection;
- text and JSON `verifying_output` reporting;
- progress-enabled and disabled output equality;
- 128 MiB bounded-memory streaming inspection and hashing;
- GCC ASan/UBSan 49-test suite, Clang cross-build, and scan-build;
- Release verification benchmark at 1,000, 10,000, and 100,000 digits.

### Definition of Done

- every CLI result is verified by default before terminal success;
- success reports canonical SHA-256 and verification manifest path;
- verification failures retain structured stage diagnostics;
- external file revalidation can compare against manifest SHA-256;
- verification does not depend on calculator internal state;
- large file structure and hashing remain bounded-memory.

### Next Contributor TODO

- Do not change the final-output or manifest format without a new version and
  compatibility decision.
- Select the next PR boundary with the user before implementation.
- If large-scale verification latency becomes a priority, benchmark parallel
  execution of independent BBP samples while keeping algorithms thread-free.

---

## PR-0028: Concurrent Storage Pipeline Implementation

### Goal

Implement concurrent storage execution without changing the PR-0027 durable
storage, lifecycle, progress, or synchronous fallback contracts.

### Approved Scope

- protect StorageManager index publication and in-flight identities while
  allowing file operations to overlap;
- remove unnecessary manager-wide serialization from async reader/writer
  workers;
- move encoded Chunk ownership into the async spill queue where safe;
- add a bounded LZ4 codec with durable round-trip and CRC validation;
- audit platform applicability without enabling unmeasured NUMA, HugeTLB, THP,
  or affinity policy by default.

### Verification

- full build and 64-test CTest suite;
- sync/async P/Q/T equality and lifecycle failure coverage;
- async multi-chunk spill/reload benchmark;
- none/LZ4 durable round-trip and metadata validation;
- `git diff --check` and documentation synchronization.

### Definition of Done

- Functional changes are separated from PR-0029 measurement-only changes.
- PR-0028 docs, checklist, changelog, and decisions match the implementation.
- The verified change set is committed and pushed before PR-0029 begins.

### Next Contributor TODO

Split out any PR-0029-only timing or benchmark changes, run the final PR-0028
verification command set, then commit and push PR-0028. Do not begin CRC,
buffer, queue, or platform tuning in this PR.

---

## PR-0029: Measurement and Bottleneck Optimization

### Goal

Build a reproducible performance baseline on top of the committed PR-0028
pipeline, identify stage-level bottlenecks, and apply only benchmark-proven
optimizations.

### Approved Sequence

1. Fix Release environment and workload/acceptance metadata.
2. Complete file, CRC32C, codec, GMP, index, and queue telemetry. **Complete**
3. Run 100 MiB/512 MiB/1 GiB single- and multi-chunk matrix with sync/async,
   worker 1/2/4/8, queue 1/4/16/64, cold/warm cache, five repetitions, and
   p50/p95 summaries.
4. Analyze CRC/read/GMP/buffer/queue/peak-RSS bottlenecks.
5. Implement bounded buffer reuse, streaming, CRC/read/GMP improvements, and
   queue adjustments one at a time.
6. Validate large-data Release, ASan/UBSan, restart/integrity, and sync/async
   equivalence.
7. Evaluate NUMA, Huge Pages, and affinity only on suitable hardware and only
   when measurements show a benefit.

Steps 1 and 2 are fixed in the PR-0029 measurement contract recorded in this
section and are implemented in the current working tree. Completion history,
architectural decisions, and task state remain in `docs/CHANGELOG.md`,
`docs/DECISIONS.md`, and `docs/CHECKLIST.md`. The bounded buffer and
CRC/read/GMP combined end-to-end before/after acceptance is complete from the
preserved 12-cell raw population. Baseline stage telemetry is unavailable, so
individual CRC/read/GMP attribution remains limited. Peak RSS, independent
Reader/Writer execution, and multi-chunk repeated cold/warm acceptance are
complete; only final stabilization and the final performance decision remain.

### Canonical PR-0029 Remaining Sequence

The remaining work is maintained in this file; separate agent-authored
`PR-0029-*` plan or report files are not required.

1. **Bounded buffer/data movement before/after — Complete**
   - Compare the PR-0028 baseline and current PR-0029 Release build on
     100/512/1024 MiB × none/LZ4 × tmpfs/ext4, five samples per cell.
   - Record store/load/total p50 and p95, encoded bytes, peak RSS,
     compression/decompression stage time, canonical bytes, and P/Q/T equality.
   - Accept only when correctness and memory are not worse and repeated p50/p95
     results support the claimed improvement. Otherwise classify it as a
     correctness/allocation optimization, not a latency speedup.

2. **CRC/read/GMP before/after — Complete as combined end-to-end evidence**
   - Reuse the same 12 single-chunk cells and compare file-read, CRC, GMP
     encode/decode, load, and total stage p50/p95 against baseline.
   - Keep SSE4.2 and portable fallback populations separate; retain CRC vectors,
     segmented chaining, and zero/signed GMP fixtures as correctness gates.
   - If stage latency does not improve repeatedly, record the change as
     correctness-only rather than claiming a speedup.

3. **Single-chunk peak RSS distribution — Complete**
   - Run the 12 single-chunk cells five times and calculate min/p50/p95/max for
     `peak_rss_mib`, with stored bytes and integrity status.
   - **Complete:** 60/60 samples succeeded. Per-cell p95 RSS was 491 MiB
     (100 MiB/LZ4) to 5,152 MiB (1 GiB/none), below the recorded PR-0028
     release baseline ranges of 591~620, 2,962~3,112, and 5,523~5,945 MiB.

4. **Independent Reader/Writer repeated matrix — Execution complete**
   - Run `reader-writer-concurrency-benchmark` for workers 1/2/4/8,
     queue 1/4/16/64, tmpfs/ext4, and none/LZ4, five repetitions per cell.
   - **Complete:** 64 cells and 320 runs completed. All runs reported
     `writer_success=8`, `reader_success=8`, and `indexed_chunks=16`; the
     raw population is preserved at
     `/tmp/givemepi-pr0029-reader-writer-matrix.tsv`. The current benchmark
     does not emit peak RSS, so memory acceptance remains separate from this
     execution result.
   - Record success counts, final index cardinality/identity, throughput,
     queue wait count/time, active time, failures, and peak RSS.
   - Do not select a worker/queue configuration from one fastest sample;
     require p50/p95 and bounded-memory evidence.

5. **Multi-chunk concurrent I/O and cold/warm acceptance — Complete**
   - Run sync and async out-of-core workloads with workers 1/2/4/8 and queues
     1/4/16/64, separated by tmpfs/ext4, none/LZ4, and cold/warm populations.
   - **Complete:** 256 cells × five repetitions, 1,280/1,280 successful.
     Every run preserved 15 spills, 15 reloads, and the expected P-bit result;
     aggregate elapsed p50/p95 was 0.368/2.124 seconds and RSS p95 was
     27 MiB. Raw samples are preserved at
     `/tmp/givemepi-pr0029-multichunk-concurrent-cold-warm-accepted.tsv`.
     Cold/warm populations showed no correctness regression.
   - Record elapsed time, spill/reload count, P-bit result, stage telemetry,
     queue telemetry, index commit, and RSS.
   - Preserve durability sync; classify ext4 sync dominance as a bottleneck,
     not as permission to weaken fsync or atomic publication.

6. **Final PR-0029 performance decision and stabilization — Complete**
   - Group raw results by compiler, CPU, filesystem, codec, and workload;
     summarize failed samples, outliers, p50/p95, RSS, integrity, and telemetry
     in the canonical changelog/checklist/decision records.
   - **Complete:** Debug build and all 64 CTest tests passed; Release
     benchmark targets rebuilt; ASan/UBSan out-of-core smoke passed for sync
     and async 1,000,000-digit runs with leak detection disabled. The final
     matrix summaries and raw populations are recorded above.
   - Marked changes as accepted, correctness-only, or deferred. NUMA/Huge
     Pages/affinity remains explicitly deferred because the current host has
     one NUMA node and no HugeTLB pool. PR-0029 is complete and committed.

#### Common acceptance gates

- Use a fixed PR-0028 baseline and current PR-0029 build with identical
  compiler, CPU, kernel, filesystem, governor, and affinity metadata.
- Keep tmpfs and ext4 populations separate and preserve raw samples with each
  summary.
- Require five samples per cell and p50/p95; a single fastest result is not
  evidence of optimization.
- Any checksum, identity, P/Q/T, restart/index, partial-file, lifecycle,
  memory-budget, or durability failure rejects the affected optimization.

### Next Contributor TODO

PR-0029 is complete and committed as `828f287`. Preserve the benchmark
acceptance decisions and do not add further optimization scope here. Begin
PR-0030 with durable checkpoint emission and verified resume; NUMA/Huge Pages/
affinity requires a separate PR, suitable hardware, and benchmark evidence.

---

## PR-0030: Durable Checkpoint Emission and Verified Resume

### Goal

Connect the existing version-1 checkpoint, integrity, quarantine, resume
planning, and progress restoration components to the production calculation
path without changing canonical checkpoint bytes or computation identity.

### Checkpoint Policy

The production path maintains a non-overlapping merge frontier. The first
completed Binary Splitting stage is committed, then later completed stages
replace the durable frontier only after `checkpoint_interval_seconds` has
elapsed. Each commit:

1. writes new parent blocks through the existing atomic block store;
2. atomically replaces the manifest with the current sorted frontier;
3. removes superseded child files on a best-effort basis.

Checkpoint publication runs on the coordinating thread after a stage joins and
before out-of-core spill can release P/Q/T values. Worker tasks never perform
filesystem I/O. Unchanged odd frontier nodes retain their existing entries and
are not rewritten.

### Resume Contract

At startup, resume uses `CheckpointResumePlanner` as the only trust boundary.
Only structurally valid, checksum-valid, manifest-consistent, and independently
verified P/Q/T blocks seed the Binary Splitting frontier. Rejected blocks are
quarantined, temporary files are ignored, and every uncovered range is
recalculated. Accepted and recalculated ranges must form one sorted, gap-free,
non-overlapping `[0, termCount)` frontier before merging.

Changing worker count, cutoff, or tasks-per-worker does not change computation
identity. A resumed run may therefore rebalance uncovered ranges while reusing
valid blocks produced by a different execution policy.

### Work Sequence

1. Add a leveled initial-frontier input and coordinator checkpoint callback to
   staged Binary Splitting.
2. Implement interval-gated frontier publication on `CheckpointStore` without
   copying GMP payloads or rereading newly written blocks.
3. Preserve accepted manifest entries and inject accepted P/Q/T blocks into the
   staged merge DAG.
4. Restore progress from accepted blocks before entering the splitting phase.
5. Wire checkpoint and quarantine directories, checkpoint enablement, interval,
   and resume enablement through TOML and CLI configuration.
6. Connect planning, writing, resumed calculation, output, and final
   verification in `pi-engine`.
7. Verify fresh, partial, complete, corrupt, identity-mismatched, and
   interrupted restart paths with and without out-of-core storage.

### Compatibility and Failure Rules

- Keep checkpoint block and manifest format version 1 byte-compatible.
- Keep `ComputationIdentity`, deterministic filenames, checksum coverage, and
  modular P/Q/T verification unchanged.
- A block write or manifest publication failure fails a checkpoint-enabled run;
  the last published manifest remains the resume authority.
- A crash before manifest replacement leaves the prior frontier authoritative.
- A crash after manifest replacement may leave obsolete child files, which are
  rejected as unexpected and quarantined on the next resume.
- Checkpoints disabled preserves the existing calculation path and result.
- Runtime storage chunks never become checkpoint or resume evidence.

### Definition of Done

- `--checkpoint` creates durable blocks and a valid frontier manifest.
- `--resume` reuses only planner-accepted blocks and recalculates only uncovered
  ranges.
- Fresh and resumed runs produce identical decimal output and SHA-256.
- Progress restoration counts only accepted blocks and newly computed terms.
- Corrupt, partial, unexpected, and identity-mismatched files cannot enter the
  merge tree.
- Checkpoint publication precedes spill and performs no worker-thread I/O.
- The full test suite and an actual interrupted-run resume smoke pass.
- README, roadmap, checklist, changelog, and decisions match the implementation.

### Out of Scope

- checkpoint or manifest format version 2;
- reuse of runtime storage chunks as checkpoint evidence;
- NUMA, Huge Pages, affinity, or unrelated storage optimization;
- changing final-output or verification-manifest formats.

### Verification

- Debug build completed for all targets.
- All 65 CTest tests passed, including the new frontier writer and seeded
  Binary Splitting coverage.
- The five changed-path tests passed under the ASan/UBSan build with leak
  detection disabled.
- A 1,000,000-digit process was terminated immediately after its first
  manifest commit, resumed from 16 accepted blocks, and produced byte-identical
  output to an uninterrupted calculation.
- Fresh and resumed 100,000-digit out-of-core runs with a forced 1 MiB memory
  budget produced identical output and verification SHA-256.

### Next Contributor TODO

PR-0030 is complete. Preserve checkpoint format version 1, computation
identity, checkpoint-before-spill ordering, and the verified-resume trust
boundary. Select the next PR boundary with the user before implementation.

---

## PR-0031: Live Checkpoint Telemetry and CLI Resume Regression

### Goal

Publish truthful checkpoint progress after every durable frontier commit and
turn the manually exercised interrupted-resume path into a permanent CLI
regression test.

### Architecture Contract

Checkpoint work completion remains a monotonic counter: blocks accepted by
verified resume plus newly calculated initial-frontier blocks. Its fixed total
is the same accepted-plus-recalculated work plan already computed before
execution.

Durable checkpoint block count is a separate gauge equal to the current
manifest entry count. It may decrease as child blocks are replaced by merged
parents. Checkpoint bytes are the exact sum of the block files referenced by
the current manifest; unreferenced files and the manifest file itself are not
included.

`lastValidatedCheckpoint` remains resume evidence only. A separate
`lastCommittedCheckpoint` records the last newly written block after successful
manifest publication. Fresh writes must never be presented as independently
validated resume input.

The frontier writer publishes one coherent checkpoint telemetry update only
after the manifest has been atomically replaced. Publication stays on the
Binary Splitting coordinating thread. Worker threads perform no filesystem I/O
or progress formatting.

The public JSON progress schema advances to version 4 for the durable-block
gauge and last-committed range. Checkpoint block and manifest format version 1,
computation identity, checkpoint-before-spill ordering, and the verified-resume
trust boundary remain unchanged.

### Work Sequence

1. Add coherent checkpoint progress publication to `ProgressTracker`.
2. Restore durable block count and bytes from verified resume input.
3. Connect `CheckpointFrontierWriter` to the tracker.
4. Publish work completion, durable block count, exact referenced bytes, and
   the last newly committed range after manifest publication.
5. Update text and JSON reporters and increment the progress schema version.
6. Add unit coverage for fresh, resumed, interval-gated, and failed commits.
7. Add an end-to-end CLI regression that interrupts after the first manifest,
   resumes with the same identity, and compares output with an uninterrupted
   calculation.
8. Reconcile implemented Chunk progress and LZ4 compression checklist entries.
9. Update the changelog, roadmap, checklist, and decision record.

### Acceptance

- Fresh JSON progress reports non-zero checkpoint work, durable block count,
  and checkpoint bytes after the first committed frontier.
- Durable block count and bytes reflect the currently published manifest after
  child replacement; failed or interval-gated commits do not change telemetry.
- Resume initializes validated work, durable count, bytes, and last-validated
  range, then live commits continue from that state without exceeding the
  fixed work total.
- The last committed range changes only after a manifest containing a newly
  written block becomes authoritative.
- The interrupted CLI run resumes successfully and produces byte-identical
  decimal output and verification hash to an uninterrupted run.
- Changed-path sanitizer tests, the complete Debug build, and the full CTest
  suite pass.

### Out of Scope

- checkpoint or manifest format changes;
- logger integration;
- benchmark or release workflow automation;
- final-output streaming;
- NUMA, Huge Pages, affinity, SIMD, ASM, or FFT optimization.

### Verification

- Built all Debug targets.
- All 66 CTest tests passed, including the real-process
  `CheckpointResumeCliTest`.
- All eight changed-path tests passed under ASan/UBSan with leak detection
  disabled.
- The automated 1,000,000-digit CLI scenario observed the first authoritative
  manifest, terminated that process, resumed with the same computation
  identity, and matched uninterrupted decimal bytes and verification SHA-256.

### Next Contributor TODO

PR-0031 is complete. Preserve the distinction between monotonic checkpoint
work, the current durable frontier, resume-validated evidence, and freshly
committed ranges. Keep progress JSON schema version 4, checkpoint/manifest
format version 1, and the verified-resume trust boundary stable. Select the
next PR boundary with the user before implementation.

---

## PR-0032: Default CLI Progress and Checkpoint Hardening

### Goal

Make the default text-mode CLI prove and display completed term and checkpoint
state reliably, and remove the misleading checkpoint work plan when resume is
enabled but checkpoint writing is disabled.

### Architecture Contract

A successful default run must finish with a terminal progress snapshot whose
completed terms equal total terms, whose checkpoint work is non-zero and
complete, and whose durable checkpoint count and bytes match files referenced
by the published manifest.

Resume-only mode (`checkpoint=false`, `resume=true`) may report verified
accepted checkpoint blocks, but it must not plan new checkpoint work because
no writer exists. With no accepted blocks its checkpoint work is `0/0`.

Interactive text output uses a compact terminal record that does not include
the full log-only storage and I/O diagnostics. Non-TTY text logs remain
newline-delimited and complete. Reporter failures are observable by the CLI
and must not be silently converted into a successful run with stale progress.

Atomic publication of a parentless relative output such as the default
`pi.txt.verify` synchronizes the current working directory; it must not try to
open an empty parent path.

Checkpoint and manifest format version 1, JSON progress schema version 4,
checkpoint-before-spill ordering, and the verified-resume trust boundary do
not change.

### Work Sequence

1. Plan recalculated checkpoint blocks only when checkpoint writing is enabled.
2. Preserve accepted resume blocks as completed work in resume-only mode.
3. Make text and JSON output-stream failures observable to the reporter runner.
4. Make the CLI reject a failed progress reporter after joining it.
5. Publish parentless relative outputs by synchronizing the current directory.
6. Add a compact TTY text record while preserving full text log records.
7. Extend the real-process CLI regression with a no-config default text run.
8. Assert final `terms=N/N`, completed checkpoint work, durable bytes, manifest,
   root checkpoint files, and the default relative verification manifest.
9. Add a resume-only/no-writer case that requires `checkpoints=0/0`.
10. Record the runtime integration checklist and verification evidence.

### Acceptance

- A no-config, no-argument run exits successfully with final text progress
  `phase=completed`, `terms=N/N`, non-zero completed checkpoint work, durable
  blocks, and bytes.
- Its working directory contains a non-empty manifest and root checkpoint
  block under the default `checkpoints/` directory.
- `--no-checkpoint --resume` with no accepted blocks reports
  `checkpoints=0/0` and creates no checkpoint files.
- Reporter failure is returned as a CLI failure instead of leaving stale
  progress behind a successful result.
- Interactive terminal records are compact; full storage and I/O diagnostics
  remain available in text log and JSON formats.
- Parentless relative outputs such as the default `pi.txt.verify` publish
  atomically and synchronize their actual current-directory parent.
- Changed-path sanitizer tests, a complete Debug build, and the full CTest
  suite pass.

### Out of Scope

- checkpoint cadence inside an unfinished Binary Splitting leaf stage;
- checkpoint or manifest format changes;
- JSON progress schema changes;
- binary version metadata or installation packaging;
- benchmark, release workflow, NUMA, Huge Pages, SIMD, ASM, or FFT work.

### Verification

- Built all Debug targets and passed all 66 CTest tests. The affected reporter
  tests passed again after output-stream failure detection was added.
- All seven changed-path tests passed under ASan/UBSan with leak detection
  disabled.
- A rebuilt no-argument executable completed 1,000,000 digits with
  `terms=70517/70517`, `checkpoints=88/88`, one durable 2,157,009-byte
  checkpoint block, a non-empty manifest, `pi.txt`, and `pi.txt.verify`.
- Direct TTY, non-TTY, JSON schema v4, resume-only `checkpoints=0/0`, and
  `/dev/full` reporter-failure smoke runs passed.

### Next Contributor TODO

PR-0032 is complete. Preserve the working-directory-relative path contract,
parentless atomic publication, writer-aware checkpoint planning, terminal
reporter failure propagation, compact TTY record, complete text logs, and JSON
schema version 4. Select the next PR boundary with the user before
implementation.

---

## PR-0033: CLI Help and Usage Errors

### Goal

Give `pi-engine` a complete `-h`/`--help` and stop silently ignoring unknown
command-line options, so a typo such as `--digit` can no longer start a
default-sized calculation.

### Architecture Contract

`CommandLine::parse` reads `argv` left to right into `CommandLineOptions`
(engaged `std::optional` fields per override) without reading
`config.toml` or touching the filesystem. `main` parses first:

- `-h` or `--help` stops parsing, writes `CommandLine::writeHelp` to stdout,
  and exits 0 before configuration loading, validation, calculation, or any
  file or directory creation.
- `CommandLineUsageError` (unknown option, missing value, value that is not
  an unsigned integer of the option's width, or a progress format other than
  `text`/`json`) prints `pi-engine: <message>` and
  `Try 'pi-engine --help' for usage.` to stderr and exits 2.
- Otherwise `ConfigLoader::load("config.toml")` runs, then
  `CommandLine::applyOverrides(config, options)` applies engaged fields and
  validates the merged configuration. Configuration, validation, calculation,
  I/O, and verification failures keep exit status 1.

The help groups exactly the supported options as Calculation, Output,
Checkpoint and resume, Progress, Out-of-core storage, Final verification, and
General, and renders built-in defaults from `include/config/Defaults.hpp`,
accepted values and units, configuration precedence, working-directory path
resolution, exit status, and examples. No `--version` option is added.
Calculation, checkpoint, storage, progress, and verification formats do not
change.

### Work Sequence

1. Split command-line handling into `parse` (syntax, usage errors, help) and
   `applyOverrides` (merge and validate).
2. Reject unknown options and parse numeric values strictly with
   `std::from_chars`, replacing the permissive `std::stoull` path for
   `--digits` and storage byte sizes.
3. Render grouped help from the shared defaults.
4. Parse before loading `config.toml` in `main`; map usage errors to exit 2.
5. Migrate `ProgressConfigTest` to the new API.
6. Add `CommandLineTest` for parsing, help content, and help/parser option
   parity.
7. Add the real-process `CommandLineCliTest` for help aliases, usage errors,
   configuration failures, and overrides of `config.toml`.
8. Update README and canonical documents.

### Acceptance

- `pi-engine -h` and `pi-engine --help` print identical help to stdout, write
  nothing to stderr, exit 0, and leave a working directory containing an
  invalid `config.toml` unchanged.
- An unknown option prints the exact two-line diagnostic to stderr, writes
  nothing to stdout, exits 2, and creates no files.
- Missing and malformed option values exit 2; an invalid `config.toml` and
  merged-configuration range failures such as `--bbp-samples 33` exit 1.
- CLI options override `config.toml`, and unset options keep its values.
- The help lists exactly the options accepted by the parser.
- A complete Debug build, the full CTest suite, and changed-path
  ASan/UBSan tests pass.

### Out of Scope

- `--version` or build metadata;
- `--option=value` syntax, short aliases other than `-h`, or abbreviations;
- CLI switches for `out_of_core`, `compression`, or `huge_pages`;
- calculation, checkpoint, storage, progress, or verification format changes.

### Verification

- Built all Debug targets and passed all 68 CTest tests, including the
  real-process `CommandLineCliTest` and existing checkpoint/resume regression.
- All four changed-path tests passed under ASan/UBSan with leak detection
  disabled.
- Direct smoke runs observed 97 help lines on stdout with empty stderr and
  exit 0, and exact two-line `--digit` diagnostics with empty stdout and exit 2.

### Next Contributor TODO

PR-0033 is complete. Keep `CommandLine::writeHelp` and
`CommandLine::parse` in step: `CommandLineTest` fails when the help lists an
option the parser rejects or omits one it accepts. Preserve parse-before-config
ordering and the 0/1/2 exit-status contract. Select the next PR boundary with
the user before implementation.

---

## PR-0034: Logger Foundation and Resume Diagnostics

### Goal

Add a synchronous, thread-safe diagnostic logger and use it to expose
checkpoint resume warnings that are currently dropped, without changing CLI
help, usage errors, exit statuses, calculation results, or progress formats.

### Architecture Contract

`pi::logging::Logger` writes one complete diagnostic record per call to a
caller-owned `std::ostream`. `LogLevel` has `debug`, `info`, `warning`, and
`error` levels; production uses a fixed `warning` threshold. Each record has
the stable form
`pi-engine: <level>: <component>: <message>\n`.

Logger writes are synchronous and serialized by an instance mutex. Carriage
returns and newlines in components or messages are written as spaces so one
call cannot inject additional records. Sink failures are contained by the
logger and never replace the calculation result or original failure.

The logger has no global singleton, macros, background thread, timestamp,
external dependency, file sink, CLI option, or TOML setting. Progress remains
the calculation telemetry channel and retains text and JSON schema version 4.
Help, usage errors, fatal errors, and successful result output keep their
existing direct output contracts.

Production logging is limited to checkpoint resume diagnostics emitted after
resume planning and before the progress reporting runner starts. Rejected
blocks, validation diagnostics, successful quarantine destinations,
quarantine-operation failures, and manifest-load failures are warnings.
Storage, scheduler, calculation, verification, and progress internals do not
gain a logger dependency.

### Work Sequence

1. Add `LogLevel` and the synchronous `Logger` API.
2. Implement threshold filtering, single-record serialization, newline
   normalization, and sink-failure containment.
3. Add behavior coverage for levels, exact records, newline normalization,
   failed sinks, and concurrent writers.
4. Register the production source and `LoggerTest` with CMake.
5. Create the production warning logger after early help handling.
6. Replace direct checkpoint rejection output and expose quarantine and
   manifest diagnostics before progress reporting starts.
7. Run targeted logger, command-line, progress, and checkpoint/resume tests.
8. Exercise a real rejected-checkpoint resume path and verify warning ordering.
9. Run the complete Debug build, full CTest suite, and changed-path
   ASan/UBSan tests.
10. Update README and canonical project documents with observed results.

### Acceptance

- Concurrent calls produce complete, non-interleaved one-line records.
- Records below the configured threshold are omitted.
- Embedded carriage returns and newlines cannot create extra records.
- A failed or throwing sink cannot throw from `Logger::write` or change the
  process exit status.
- Rejected checkpoints, quarantine outcomes, and manifest-load failures are
  reported before the progress runner starts.
- `--help`, usage errors, fatal errors, successful stdout, and exit statuses
  0/1/2 are unchanged.
- A normal successful `--no-progress` run without warnings leaves stderr empty.
- Text progress records and JSON progress schema version 4 are unchanged.
- The new unit test, affected CLI regressions, the complete Debug build, the
  full CTest suite, and changed-path ASan/UBSan tests pass.

### Out of Scope

- log-level CLI or TOML configuration;
- asynchronous logging, rotation, timestamps, or file/network sinks;
- structured JSON logs or progress schema changes;
- logging inside storage, scheduler, calculation, verification, checkpoint,
  or progress libraries;
- routing help, usage, fatal, successful-result, or verification output through
  the logger;
- benchmark, release workflow, NUMA, Huge Pages, SIMD, ASM, or FFT work.

### Verification

- Built all Debug targets and passed all 69 CTest tests.
- `LoggerTest`, `CommandLineCliTest`, `ProgressReportingRunnerTest`, and
  `CheckpointResumeCliTest` passed under combined ASan/UBSan with leak
  detection disabled.
- A fresh 1,000-digit `--no-progress` run produced no stderr. After its
  checkpoint block was corrupted, the resumed run emitted rejection,
  structural validation, and quarantine warnings, then completed.
- The real-process checkpoint/resume regression corrupts a published manifest,
  requires the manifest warning before the first progress record, and requires
  the run to finish with `phase=completed`.

### Next Contributor TODO

PR-0034 is complete. Preserve the separation between diagnostic warnings,
progress telemetry, exact CLI errors, and successful stdout. Keep resume
warnings before progress starts, and do not add logger configuration or inject
the logger into library subsystems without a separate approved PR boundary.

---

## PR-0035: Informational Benchmark Workflow

### Goal

Build and run the existing benchmark executables on demand in GitHub Actions,
preserve their raw output as an artifact, and establish a repeatable
measurement gate before later optimization work.

### Architecture Contract

Benchmark automation is separate from correctness CI. It runs only through
`workflow_dispatch`, uses a Release build with `ENABLE_BENCHMARKS=ON`, and
never gates a pull request on elapsed-time thresholds from a shared runner.
Process failures and benchmark-internal correctness failures remain fatal.

The existing no-argument benchmark workloads and CSV schemas remain stable.
Fixed-workload benchmarks accept one exact optional `--quick` argument for a
bounded CI profile. Unknown or additional arguments fail explicitly.

### Work Sequence

1. Add bounded `--quick` profiles to the fixed-workload Chudnovsky and final
   verification benchmarks without changing their default profiles.
2. Add a manually dispatched Release benchmark workflow.
3. Build all five existing benchmark targets.
4. Run each target with bounded inputs and capture its raw output.
5. Upload the benchmark output as a workflow artifact.
6. Reconcile benchmark, platform, memory, Big Integer, and assembly checklist
   entries with the implementation that already exists.
7. Document the informational shared-runner measurement contract.
8. Run all benchmark quick profiles, the complete Debug build, and CTest.

### Acceptance

- Manual workflow dispatch builds all benchmark executables in Release mode.
- All five benchmark executables run with bounded inputs and preserve raw
  output in one uploaded artifact.
- Chudnovsky quick mode still compares sequential and parallel output.
- Final-verification quick mode still requires complete verification success.
- Shared-runner timing values are informational and have no pass/fail
  threshold.
- Existing no-argument benchmark behavior and CSV schemas do not change.
- Invalid fixed-workload benchmark arguments fail with a diagnostic.
- The normal Debug build and complete CTest suite remain successful.

### Out of Scope

- scheduled benchmark runs or pull-request benchmark gates;
- performance-regression thresholds or baseline comparison;
- release workflow, packaging, or installation;
- new benchmark programs;
- SIMD, assembly, NUMA, Huge Pages, or calculation optimization;
- logger configuration or library-subsystem logging.

### Verification

- Configured a GCC Release build with `ENABLE_BENCHMARKS=ON` and built all
  five benchmark targets.
- Ran both `--quick` profiles, an 8 MiB storage round trip, a 1,000,000-digit
  asynchronous out-of-core merge with 15 spills and reloads, and a two-reader/
  two-writer concurrency workload successfully.
- Both fixed-workload benchmarks rejected `--bogus` with empty stdout, a
  usage diagnostic on stderr, and a non-zero exit status.
- Built all Debug targets and passed all 69 CTest tests.
- Parsed the workflow as YAML. `actionlint` was not available locally; remote
  `workflow_dispatch` remains the GitHub-surface verification.

### Next Contributor TODO

PR-0035 is complete. Keep shared GitHub runner timing informational and retain
the existing no-argument benchmark profiles and CSV schemas. Select any
optimization or release-workflow boundary with the user before implementation.

---

## PR-0036: Tag-Gated Source Release Workflow

### Goal

Publish a tested, checksummed source archive from an exact version tag while
keeping non-portable prebuilt binaries out of the release contract.

### Architecture Contract

The release version is `0.13.0`, following the existing `0.12.0` changelog
history. `CMakeLists.txt`, the `v0.13.0` tag, and the
`## [0.13.0] - PR-0036` changelog heading must agree.

The workflow runs on `v*` tag pushes. Read access is the default; only the
publish job receives `contents: write`. It builds the tagged source in Release
mode, runs the complete CTest suite in a separate Debug build, creates a
prefixed source archive, verifies its SHA-256, repeats both builds from the
extracted archive, and publishes only the source archive plus `SHA256SUMS`.

The changelog keeps one top-level `[Unreleased]` section. Pull requests append
there; a release moves accumulated entries under one version heading. This
supersedes the historical requirement to create a version heading for every
pull request.

### Work Sequence

1. Advance the CMake project version from stale `0.1.0` to `0.13.0`.
2. Cut the current changelog into `[0.13.0] - PR-0036`, retain an empty
   `[Unreleased]` section, and remove the stale duplicate `[Unreleased]`
   section after the historical version entries.
3. Record the release-version and changelog policy in a superseding ADR.
4. Add a tag-gated source release workflow with narrow permissions.
5. Reject tag, project-version, or changelog-version mismatch before building.
6. Build the checkout in Release mode and run the full suite from a Debug build.
7. Create `givemepi-0.13.0.tar.gz` and `SHA256SUMS`.
8. Extract the archive into a clean directory, then repeat both builds.
9. Publish changelog-backed release notes and the two release assets.
10. Update README, roadmap, checklist, and changelog.

### Acceptance

- A tag other than `v0.13.0` fails before packaging.
- Missing or mismatched changelog version evidence fails before packaging.
- The tagged checkout and extracted source archive both build in Release mode,
  and separate Debug builds pass the complete CTest suite.
- `sha256sum --check SHA256SUMS` succeeds.
- The archive contains tracked source, vendored toml++, both licenses,
  `config.toml.example`, README, and canonical documents.
- Only the publish job has `contents: write`.
- The GitHub release contains only the source archive and checksum file.

### Out of Scope

- prebuilt Linux, Windows, or macOS binaries;
- `install()` rules, CPack, `.deb`, or `.rpm` packages;
- a `--version` CLI option or generated version header;
- signing, provenance attestation, or package registries;
- changing `-march=native`, runtime dependencies, or optimization behavior;
- SIMD, assembly, NUMA, Huge Pages, or performance tuning.

### Verification

- The PR-0035 benchmark workflow completed remotely and uploaded its
  `benchmark-results` artifact.
- Built the checkout in Release mode and built and passed all 69 CTest tests
  in Debug mode with the agent CPU limit set to 13 of 22 logical cores.
- Created the prefixed source archive with the exact pinned toml++ commit,
  verified `SHA256SUMS`, required files, and all 577 archive entries.
- Extracted the archive into a clean directory, completed its Release build,
  and built and passed all 69 Debug CTest tests at the same CPU limit.
- `actionlint` 1.7.12, YAML parsing, release-note extraction, exact version
  agreement, and mismatched-tag rejection passed.
- Final review found one fixed portability issue in the release-heading gate;
  it now accepts the matching semantic version with any valid release PR ID.

### Next Contributor TODO

PR-0036 is complete. Publish binaries only after portable architecture,
runtime dependency, and installation contracts are approved. Replace
side-effecting assert-based tests before treating Release CTest as meaningful.

---

## PR-0037: End-to-End and Phase ETA

### Goal

Keep an ETA visible from the first usable progress sample through output
verification, while allowing operators to select total completion ETA, current
phase ETA, or both.

### Architecture Contract

Progress schema version 5 carries separate total and current-phase remaining
durations. `--eta-total` and `--eta-phase` are independent CLI selections; when
both are present both values are reported. With neither option, total ETA is
the default. Text output omits unselected values. JSON keeps both stable keys
and represents an unselected or not-yet-estimable value as `null`.

ETA remains derived telemetry. Raw phase and work counters are authoritative.
Resume-restored work is recorded as an initial baseline rather than observed
throughput. Splitting, weighted merge work, fixed-point finalization, output
bytes, final-verification file work, verification stages, and BBP samples
publish bounded progress units. Disabled stages are removed from the total work
plan. The estimator uses observed work rates, resets the phase estimate at each
transition, continuously corrects the total estimate, and returns unknown until
the configured minimum sample is available. A completed run reports zero; a
failed run reports no ETA. Checkpoint validation remains before tracker and
reporter construction so resume warnings retain their established ordering.

### Work Sequence

1. Extend the progress work plan, tracker, snapshot, and schema with
   phase-specific and total pipeline work.
2. Publish actual progress from Binary Splitting and merging, finalization,
   output writing, and final verification; exclude resume-restored work from
   observed rates.
3. Derive independent total and current-phase ETA values from immutable
   snapshots without moving formatting into computation workers.
4. Add independent `--eta-total` and `--eta-phase` CLI selections and document
   the default-total behavior.
5. Update text and JSON reporters for the two ETA values and schema version 5.
6. Cover startup, resume, phase transitions, writing, verification,
   verification-disabled, short-run, failure, completion, and dual-selection
   behavior.
7. Run changed-path tests and one real JSONL CLI scenario through
   `verifying_output` under the agent CPU limit.
8. Update README, roadmap, checklist, decisions, and changelog.

### Acceptance

- The default progress output reports total completion ETA after the minimum
  usable sample and continues through `verifying_output`.
- `--eta-total` reports only total ETA, `--eta-phase` reports only phase ETA,
  and both options report both.
- Resume-restored work does not appear as newly completed work or inflate the
  observed rate.
- Phase ETA resets at each phase boundary; total ETA remains an end-to-end
  estimate and is corrected as actual work completes.
- Verification-disabled runs exclude verification work and transition from
  writing to completed.
- JSON schema version 5 exposes stable, independently nullable total and phase
  ETA fields.
- Completed records report zero selected ETA values; failed records report
  null selected ETA values.
- Progress workers publish counters only and never format output or perform
  additional blocking I/O.

### Out of Scope

- Persisting timing profiles across processes or machines;
- promising deadline accuracy or a monotonic ETA under changing storage and
  arithmetic throughput;
- Reordering resume diagnostics after progress output or estimating work that
  completes before tracker construction;
- changing checkpoint, verification-manifest, or decimal-output formats;
- performance optimization unrelated to progress instrumentation.

### Verification

- Configured and built all Debug targets with the agent limit set to 13 of 22
  logical cores.
- Passed all 69 CTest tests with 13-way test parallelism.
- A 1,000,000-digit dual-ETA JSONL run passed final verification and published
  non-null total and phase ETA during `verifying_output`; both were zero in the
  completed record.
- A 5,000,000-digit dual-ETA JSONL run published non-null total and phase ETA
  during merging, finalization, and `verifying_output`, then completed with
  both values at zero.
- A verification-disabled JSONL run omitted `verifying_output` and completed
  with both selected ETA values at zero.
- Final review found no correctness, concurrency, overflow, schema, or
  consumer-visible issue. The live JSONL scenario caught one missing
  `storage_io` closing brace; the reporter and its regression assertion were
  corrected before the complete test run.

### Next Contributor TODO

PR-0037 is complete. Treat total ETA as a heuristic that may move in either
direction when arithmetic, storage, or verification throughput changes. Keep
raw counters authoritative, preserve the independent CLI selections and JSON
fields, and do not move resume diagnostics behind progress output without a
separate approved contract change.

## PR-0038: Evidence-Driven Total ETA

### Goal

Stop total and phase ETA from rising merely because wall time passes between
coarse progress publications, and price later phases from measured phase
throughput instead of a splitting-dominated whole-run average.

### Root Cause

PR-0037 recomputes ETA statelessly as `remaining * elapsed / completed`.
With counters fixed between publications, both terms grow linearly with
time (slope `(U-u)/(u-u0) + W_later/W_new > 0`). Merge units are serial work
while planned merge cost divides by level parallelism, verification decimal
import is quadratic but counted linearly, and later phases are converted with
`elapsed / normalized work` that splitting dominates.

### Scope

- Tracker records the steady time of the latest current-phase unit and the
  wall duration of every closed phase.
- Phase rate is measured up to the latest unit: `r = (u-u0)/(phi-s)`,
  `R_phase = (U-u)/r - s`; negative remaining reports overdue (null), never 0.
- Calibration `k` = closed phase durations / closed newly completed planned
  cost; before any phase closes, the current phase's measured cost rate.
- `R_total = R_phase + k * W_later`; without a phase sample the current phase
  uses `k * W_cur - phi`. The whole-run global-rate branch is removed.
- Merge progress units are divided by `min(workers, pairs at level)` in both
  planning and completion.
- Verification decimal-import units are weighted to the quadratic import cost.
- JSON schema v6 adds `eta_total_basis`
  (`measured|calibrated_plan|overdue|unknown`) and `eta_phase_overdue`; text
  renders `eta_total=overdue` / `eta_phase=overdue`; compact lines stay within
  80 columns.

### Out of Scope

- Smoothing or forcing monotonic ETA;
- optimizing the quadratic decimal import (needs a benchmark-first PR);
- persisting timing profiles across runs.

### Verification

- Deterministic metric tests with injected durations: stall decreases ETA by
  exactly elapsed time, overdue, slowdown step, calibration independent of
  elapsed, finalizing before first sample, resume exclusion, constant merge
  rate across levels, completed/failed/disabled verification.
- Full Debug build and CTest at 13 of 22 logical cores.
- 5,000,000-digit dual-ETA JSONL run: total ETA does not rise between unit
  publications and is zero on completion.

### Verification Results

- Built every Debug target and passed all 69 CTest tests with 13-way
  parallelism.
- A 5,000,000-digit, 13-thread dual-ETA JSONL run completed and passed final
  verification in 59.4 seconds. Across 179 adjacent records with unchanged
  counters, neither total nor phase ETA rose; phase ETA decreased by the
  elapsed interval to nanosecond precision. Both ETAs were zero in the
  completed record.
- ETA rose on 2 of 88 counter-changing pairs, reflecting newly measured slower
  work rather than time alone. The final BBP samples reported `overdue` instead
  of a false zero.
- Review found and fixed the final-unit publication gap: a phase with all units
  complete now reports zero, not overdue, while waiting to transition. Its
  focused regression test passed.

### Next Contributor TODO

PR-0038 is complete. Treat upward correction on new work evidence as expected.
The BBP verification samples remain relatively underweighted; benchmark and
model them in a separate optimization or progress-accounting PR.

## PR-0039: Configurable Text ETA Units and Precision

### Goal

Render human-readable text ETA in seconds, minutes, or hours with a configurable
fixed decimal precision while preserving JSON's exact nanosecond contract.

### Scope

- Add `--eta-unit <auto|min|hour>`; default `auto`.
- In `auto`, use `sec` below 60 seconds, `min` below 60 minutes, and `hour`
  otherwise. Fixed `min` and `hour` never switch units.
- Add `--eta-precision <digits>`; default 2 and accept 0 through 9, matching
  nanosecond resolution.
- Apply the same format to selected total and phase ETA in terminal and log
  text output. `unknown` and `overdue` remain unchanged.
- Keep JSON ETA values as integer nanoseconds with schema version 6 unchanged.
- Add parser, validation, help, reporter, and CLI regression coverage.

### Out of Scope

- TOML keys for ETA display formatting;
- changing ETA calculation or JSON values;
- locale-dependent decimal separators.

### Verification

- Focused unit and CLI tests cover automatic thresholds, fixed minute/hour
  output, precision 0/2/9, invalid units, and precision outside 0 through 9.
- Full Debug build and CTest use 13 of 22 logical cores.
- Live text progress confirms defaults and both fixed units.

### Verification Results

- `TextProgressReporterTest` covers `auto` at 1 ns below and exactly 60 seconds
  and 60 minutes, fixed `min` and `hour`, precision 0, 2, and 9, both ETAs in
  log mode, rejected precision 10, and unchanged `unknown`/`overdue` output.
  Parser, help, CLI-process, override, default, TOML-ignored, and
  post-merge precision validation tests cover the new options.
- Built every Debug target and passed all 69 CTest tests with 13-way
  parallelism.
- Three non-TTY 5,000,000-digit, 13-thread text runs passed final
  verification. The default switched from `eta_total=1.11min` while finalizing
  to `eta_total=26.30sec` while verifying and completed at
  `eta_total=0.00sec eta_phase=0.00sec`. `--eta-unit min` stayed in minutes
  (`0.75min` through `0.00min`), and `--eta-unit hour --eta-precision 4`
  rendered `eta_total=0.0159hour` through `eta_total=0.0000hour`.
- Final review found one compact-terminal width regression at precision 9.
  Compact rendering now omits checkpoint counts and then the term fraction
  only when needed to stay below 80 columns; its focused regression test
  covers two selected fixed-hour ETAs at precision 9.

### Next Contributor TODO

PR-0039 implementation, documentation, and verification are done; the
developer commit and push remain. Keep ETA unit and precision as
command-line-only presentation policy in the shared `EtaFormat` type: do not
change JSON nanosecond values or schema version 6 for display needs, and add
TOML keys only through a separately approved plan.

---

## PR-0040: Runtime Storage Run Isolation

### Goal

Isolate production runtime storage by calculation run so a fresh or resumed
process cannot consume stale runtime chunks and two processes cannot share one
active runtime-storage namespace.

PR-0039 must be committed and pushed before PR-0040 begins. PR-0040 must be
completed before PR-0041 begins.

### Architecture Contract

- Acquire a production runtime-storage base lock before creating or opening the
  active namespace, and hold it for the complete runtime-storage lifetime.
- Start each calculation with a clean active runtime-storage namespace. Runtime
  chunks remain temporary working data and never become checkpoint or resume
  evidence.
- Report indexed duplicate identities separately from identities already
  protected as in-flight operations. Diagnostics must identify which condition
  rejected the operation.
- Leave checkpoint blocks, manifests, computation identity, format version 1,
  checkpoint-before-spill ordering, and verified-resume behavior unchanged.

### Work Sequence

1. Define ownership and lifetime for the production runtime-storage base lock.
2. Clean and initialize one active namespace only while that lock is held.
3. Route production runtime chunk and index paths through the isolated active
   namespace.
4. Split duplicate diagnostics into indexed and in-flight identity failures.
5. Add regression coverage for an interrupted run resumed after runtime spill;
   only checkpoint evidence may seed the resumed frontier.
6. Add concurrent-use coverage proving a second process cannot use the same
   runtime-storage base while the first holds it.
7. Run the focused regressions, complete build and test gates, then update
   completion documentation without changing checkpoint contracts.

### Acceptance

- A production run holds the runtime-storage base lock for the complete
  runtime-storage lifetime.
- A new active namespace is clean and cannot expose stale chunks or an old
  runtime index to the calculation.
- Indexed duplicates and in-flight duplicates produce distinct, precise
  diagnostics.
- An interrupted post-spill run resumes from its checkpoint frontier without
  treating runtime chunks as resume evidence.
- Concurrent use of one runtime-storage base is rejected deterministically.
- Checkpoint files and format version 1 bytes are unchanged.

### Verification Results

- Built every Debug target with 13 of 22 logical CPUs.
- Passed all 70 CTest tests, including the focused
  `RuntimeStorageSessionTest`, `StorageManagerTest`, and
  `CheckpointResumeCliTest` regressions.
- Verified an interrupted post-spill run resumes from checkpoint evidence while
  the stale runtime namespace is discarded.

### Next Contributor TODO

PR-0040 is complete. Preserve the stable base lock, ephemeral active namespace,
and checkpoint/runtime-storage separation while implementing PR-0041.

### Out of Scope

- checkpoint promotion across precision identities;
- checkpoint or manifest format changes;
- making runtime chunks resumable;
- unrelated storage performance optimization.

---

## PR-0041: Cross-Precision Checkpoint Promotion

### Goal

Promote a compatible, explicitly selected lower-precision checkpoint frontier
into a higher-precision target calculation without weakening checkpoint
validation or mutating the source checkpoint set.

### Architecture Contract

- Promotion requires an explicit source manifest. Discovery or implicit
  selection of a source checkpoint population is not allowed.
- Source validation is read-only and all-or-nothing. Any incompatible,
  structurally invalid, checksum-invalid, manifest-inconsistent, or
  mathematically invalid source entry rejects the complete promotion; source
  files are never quarantined, rewritten, renamed, or removed.
- Compatible lower-precision P/Q/T blocks are imported as the target
  calculation's initial frontier. Uncovered target ranges are calculated under
  the target identity.
- Checkpoint block and manifest format version 1 is retained.
- The first complete target frontier commit re-encodes imported P/Q/T blocks
  under the target computation identity before publishing the target manifest.
  No target manifest may reference source-identity blocks.
- Existing planner-accepted blocks in the target checkpoint population conflict
  with promotion. The run must reject the mixed authority rather than merge
  accepted target blocks with the promoted source frontier.

### Work Sequence

1. Add an explicit source-manifest input and keep it separate from the target
   checkpoint and quarantine paths.
2. Validate the complete source manifest and every referenced block through a
   read-only, all-or-nothing promotion path.
3. Check lower-to-higher precision compatibility and construct the target
   initial frontier from compatible source P/Q/T plus uncovered target ranges.
4. Reject promotion when the target resume planner has accepted any existing
   target block.
5. On the first complete target frontier commit, re-encode imported blocks with
   the target identity and atomically publish a format-version-1 target
   manifest.
6. Add source immutability, validation failure, target-conflict, mixed-frontier,
   and first-target-commit regression coverage.
7. Run a Release cold-versus-promoted benchmark gate and accept promotion only
   with identical target output and verification evidence plus a measured
   benefit.

### Acceptance

- Promotion cannot begin without an explicit source manifest.
- Source validation either accepts the complete compatible source frontier or
  imports none of it, and all source bytes remain unchanged on success and
  failure.
- Compatible lower-precision P/Q/T becomes the target initial frontier while
  uncovered ranges are calculated for the target.
- Any planner-accepted target block conflicts with promotion and rejects the
  run before frontier mixing.
- The first complete target frontier commit contains only target-identity,
  format-version-1 blocks and a target-identity manifest.
- A Release cold-versus-promoted benchmark demonstrates identical target
  decimal output and final verification evidence; performance acceptance is
  based on the measured comparison.

### Out of Scope

- checkpoint or manifest format version 2;
- source checkpoint mutation or quarantine;
- implicit source discovery;
- combining promotion with existing accepted target blocks;
- runtime-storage reuse as checkpoint evidence.

### Verification Results

- Built every Debug target and passed all 71 CTest tests with 13 of 22
  logical cores. `CheckpointPromotionCliTest` covers cold-identical output,
  promoted-term seeding, source byte immutability, target-only identity,
  accepted-target conflict, same-directory, bare-filename, corrupt-source, and
  non-lower-precision rejection.
- Review fixes: bare manifest filenames resolve to the working directory; a
  source in the target checkpoint directory is rejected before the target
  resume planner can quarantine it; validation messages use the
  `Config validation failed:` style.
- The first Release benchmark showed no benefit: the large seed was merged
  with small blocks at every level. The approved fix adds
  `BinarySplitter::planMergeLevel`, which carries nodes larger than twice the
  level median and keeps the previous pairing on uniform levels.
- Release, 13 threads, 5,000,000 to 10,000,000 digits, five alternating A/B
  samples without verification: median wall 6.76 s promoted versus 7.58 s
  cold; promoted merge 2.51 s versus 3.86 s with the previous pairing. Output
  and verification SHA-256 matched in every sample. With verification both
  took 171-173 s because verification dominates.

### Next Contributor TODO

PR-0041 is complete. An interrupted promoted run resumes without
`--promote-checkpoint-manifest`. Promotion benefit is bounded by the seeded
share of splitting and merge work; finalization and verification are
unchanged.

---

## ETA `unknown` Operator FAQ

### Goal

Explain why a large calculation can continue to report `eta_total=unknown`
before the first usable progress sample, without changing ETA behavior.

### Approved Scope and Work Sequence

1. Add an operator-facing FAQ immediately after the existing README ETA
   contract.
2. Define the minimum sample conditions and the leaf-block publication
   boundary.
3. Explain the `worker count * tasksPerWorker` block-count cap, including the
   default `tasksPerWorker` value of 4.
4. Add a phase and counter diagnostic table, distinguish `unknown` from
   `overdue`, and identify CPU utilization plus active tasks as supporting
   evidence that work is continuing.
5. Update the checklist and changelog, then verify the statements against the
   implementation and build the project.

### Out of Scope

- ETA calculation, sampling thresholds, and progress publication changes;
- new configuration or command-line options;
- a separate troubleshooting document;
- architecture or roadmap changes.

### Acceptance

- The FAQ appears between the ETA contract and current implementation status.
- Every numeric default and condition matches the implementation.
- The existing project build succeeds.

### Verification Results

- Confirmed the one-unit and one-second sample thresholds in
  `ProgressMetrics`, leaf-block completion publication in `BinarySplitter`,
  and the default `tasksPerWorker` value of 4.
- Confirmed the FAQ is directly between the ETA contract and current
  implementation status.
- Built every existing target successfully with 13 of 22 logical cores.

### Next Contributor TODO

Keep this FAQ synchronized if ETA sampling thresholds, split block sizing, or
progress publication boundaries change. No follow-up is required for this
documentation task.

## PR-0042: Computation Memory Budget and Fail-Fast Admission

### Status

Approved on 2026-11-16. Not started. Numbered after the already planned
PR-0040 and PR-0041.

### Background

A 50,000,000,000-digit run (22 workers, 88 leaf blocks of about 40 million
terms) was killed by the kernel global OOM killer after 14 minutes in
`splitting` with `terms=0`: about 1.4 GiB RSS plus 27.4 GiB zram swap, with
swap exhausted. `storage_memory_budget_bytes` only governs resident merge
nodes after the leaf stage; leaf GMP working sets, finalization, the decimal
output string, and verification are unbudgeted. At this size finalization
alone needs `10^(2W)` (38.67 GiB) and the decimal string 46.57 GiB, above the
30 GiB of physical RAM.

### Goal

Never let a calculation drive the host into OOM. Estimate every stage's peak
resident memory before work starts, and reject a run whose required minimum
exceeds a computation memory budget with a precise diagnostic and exit
status 1 instead of a SIGKILL.

### Scope

- New computation memory budget option.
- Deterministic per-stage peak-memory estimate (`MemoryPlan`).
- Fail-fast admission at startup, before execution, and before each merge
  level and finalization.
- No change to arithmetic, checkpoint format, storage format, or output.

### Fixed Design Decisions

Every value below is binding. A contributor must not substitute another
constant, formula, name, or check point without a recorded plan change.

#### D1. Configuration and CLI

- `Config::memory_budget_bytes` (`std::uint64_t`), default
  `defaults::MEMORY_BUDGET_BYTES = 0`. `0` means automatic.
- TOML key `memory_budget_bytes`, loaded with `load_value` next to the
  storage keys in `ConfigLoader.cpp`.
- CLI `--memory-budget-bytes <bytes>`, parsed with
  `parseUnsigned<std::uint64_t>` into
  `CommandLineOptions::memory_budget_bytes`; malformed value is a
  `CommandLineUsageError` (exit 2).
- Help line in the "Out-of-core storage" group:
  `--memory-budget-bytes <bytes>  computation memory limit; 0 = automatic`
  with the default shown as `0 (automatic)`.
- `config.toml.example` documents the key under a new "Memory budget" comment
  in the Memory section.
- `validateConfig`: when `memory_budget_bytes != 0`, `out_of_core_enabled`,
  and `storage_memory_budget_bytes > memory_budget_bytes`, throw
  `std::runtime_error("Config validation failed: storage memory budget must not exceed the computation memory budget.")`.

#### D2. Budget Resolution

Implemented in `pi::memory::ComputationMemoryBudget`
(`include/memory/ComputationMemoryBudget.hpp`,
`src/memory/ComputationMemoryBudget.cpp`):

- `detectedMemoryBytes()`:
  1. `physical = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE)`.
  2. Read `/proc/self/cgroup`; take the line starting with `0::`; read
     `/sys/fs/cgroup<path>/memory.max`. If it parses as a decimal integer,
     `detected = min(physical, value)`; if it is `max`, missing, or unreadable,
     `detected = physical`. cgroup v1 is not consulted.
  3. Swap and `MemAvailable` are never used.
- `resolve(configured, detected)` (pure, unit-tested):
  - `configured > detected`: throw `std::runtime_error("memory budget exceeds detected memory: budget=<B> bytes detected=<D> bytes")`.
  - `configured != 0`: return `configured`.
  - Otherwise `reserve = max(2 GiB, ceil(detected / 10))`; if
    `reserve > detected / 2` then `reserve = detected / 2`; return
    `detected - reserve`.
- Reference: this 31.4 GiB host resolves to about 28.3 GiB.
- In automatic mode, if `out_of_core_enabled` and
  `storage_memory_budget_bytes` exceeds the resolved budget, admission throws
  `MemoryBudgetExceeded` with stage `storage_budget`.

#### D3. Size Formulas

All arithmetic is saturating `std::uint64_t`. Implemented as public functions
in `include/memory/MemoryPlan.hpp`:

- `limbBytes(bits) = 8 * ceil(bits / 64) + 16`.
- `decimalIntegerBytes(d) = limbBytes(ceil(d * 3322 / 1000) + 64)`.
- `nodeIntegerBits(start, end) = (end - start) * (3 * bit_width(end) + 64) + 128`.
- `nodeBytes(start, end) = 3 * limbBytes(nodeIntegerBits(start, end))`.
- `processBaseBytes = 256 MiB`, added to every stage.
- `D = requestedDigits`, `W = workingDigits`, `R = nodeBytes(0, termCount)`,
  `q = R / 3`, `S = decimalIntegerBytes(2 * W + 5)`,
  `s = decimalIntegerBytes(W + 3)`, `n = decimalIntegerBytes(D)`.

#### D4. Stage Peaks

Stages are evaluated in this order; each value is `processBaseBytes` plus:

1. `leaf_split`: frontier entries are the seeds plus the pending leaf blocks.
   `C = min(workers, pending blocks)`.
   `Σ nodeBytes(all entries) + Σ over the C largest pending blocks of 2 * nodeBytes(block)`.
   Then `max(that, Σ nodeBytes(entries) + ioExtra(entries))`.
2. `merge_level_<L>` for every level produced by replaying
   `BinarySplitter::planMergeLevel` (size-aware adjacent pairs, carried
   nodes; PR-0041). With inputs `I`, parents `P`, `C = min(workers, |P|)`:
   `Σ nodeBytes(I) + Σ nodeBytes(P) + Σ over the C largest parents of nodeBytes(parent)`.
   Then `max(that, Σ nodeBytes(P plus carried nodes) + ioExtra(outputs))`.
   The storage budget earns no reduction, because `prepareMergeNodes` reloads
   the whole level.
3. `finalization`: `R + max(2 * S, S + 3 * (s + q))`.
4. `decimal_format`: `R + 2 * (s + q) + 2 * (D + 2)`.
5. `output_write`: `D + 2`.
6. `verification`, only when enabled: `(D + 2) + 6 * n`.

`ioExtra(set) = max(checkpoint_enabled ? maxNode : 0, out_of_core_enabled ? 2 * maxNode : 0)`,
where `maxNode` is the largest `nodeBytes` in the set.

#### D5. Leaf Layout Single Source

Add `static std::vector<BinaryFrontierSpan> BinarySplitter::planFrontier(std::size_t start, std::size_t end, std::size_t workers, const ParallelSplitOptions&, std::span<const BinaryFrontierSpan> seeds)`
with `struct BinaryFrontierSpan { std::size_t start; std::size_t end; bool seeded; }`.
`splitParallel` must use it for its gap/block layout, so the estimate and the
executed layout cannot diverge. `ProgressCostModel` is unchanged.

#### D6. Admission API and Errors

- `pi::memory::MemoryBudgetExceeded : std::runtime_error`.
  `what()` is exactly
  `memory budget exceeded: stage=<stage> required=<R> bytes (<r> GiB) budget=<B> bytes (<b> GiB)`,
  with GiB in classic-locale fixed notation, two decimals. The existing
  `main` catch prints it as `pi-engine: <what>` and exits 1.
- `pi::memory::MemoryPlan::create(const MemoryPlanInput&)` computes all stage
  peaks. `MemoryPlanInput` holds `PrecisionPlan`, the frontier spans, the
  worker count, `checkpoint_enabled`, `out_of_core_enabled`, and
  `verification_enabled`.
- `MemoryPlan::enforce(std::uint64_t budget)` throws for the first stage in D4
  order whose peak exceeds `budget`. A successful check prints nothing.
- `pi::binary::BinaryMemoryGuard` in `BinarySplitter.hpp`:
  `virtual void admitMergeLevel(std::span<const BinaryNode> nodes, std::uint32_t level) = 0;`
  New trailing `BinaryMemoryGuard* memoryGuard = nullptr` on the full
  `splitParallel` overload, called before `prepareMergeNodes` each level.
  Resident nodes use actual size (`8 * (|size(P)| + |size(Q)| + |size(T)|) + 48`);
  nodes without values and planned parents use `nodeBytes`.
- `pi::chudnovsky::CalculationMemoryGuard : binary::BinaryMemoryGuard` in
  `ChudnovskyCalculator.hpp` adds
  `virtual void admitFinalization(const binary::BinaryNode& root) = 0;`.
  New trailing `CalculationMemoryGuard* memoryGuard = nullptr` on
  `calculateParallel`, called after the split and before `makeResult`.
  `R` and `q` then use the root's actual size.
- `pi::memory::MemoryAdmission` implements `CalculationMemoryGuard` from the
  resolved budget and the planning inputs.
- Run LSP references before changing the exported signatures; migrate every
  caller.

#### D7. Check Points in `main`

1. Startup: right after `applyOverrides` and `PrecisionPolicy::create`,
   before `CheckpointResumePlanner`. Uses a fresh full-range frontier
   (no seeds) and the resolved worker count. No file is read or written
   before this check.
2. Pre-execution: after resume and promotion planning, before
   `ProgressTracker`, runtime-storage session, checkpoint writer, and
   scheduler creation. Uses the actual seeds and gaps.
3. Merge levels and finalization: through `MemoryAdmission`. A rejection here
   follows the existing failure path (scheduler stop, progress failed,
   exit 1); durable checkpoints remain for resume with a larger budget.
4. `output_write` and `verification` depend only on `D`, so the startup check
   is final for them; no runtime recheck.
5. No runtime RSS polling.

#### D8. Calibration Rule

Step 10 measures peak RSS (`getrusage(RUSAGE_CHILDREN).ru_maxrss`) of a Release
build at 1,000,000, 10,000,000, and 100,000,000 digits with 13 threads.
Required: `measured <= estimated peak` and `estimated peak <= 4 * measured`.
If an estimate is too low, raise only the multiplier constants in D3/D4 and
record the new values here; never remove a term.

### Work Sequence

1. File check: `include/config/Defaults.hpp`, `include/config/Config.hpp`,
   `src/config/Config.cpp`, `src/config/ConfigLoader.cpp`,
   `include/config/CommandLine.hpp`, `src/config/CommandLine.cpp`.
2. File check: `src/main.cpp`, `src/chudnovsky/ChudnovskyCalculator.cpp`,
   `src/chudnovsky/PrecisionPolicy.cpp`, `src/binary/BinarySplitter.cpp`,
   and the PR-0040/PR-0041 changes to these files.
3. Provide a user-run script creating `include/memory/MemoryPlan.hpp`,
   `src/memory/MemoryPlan.cpp`, `include/memory/ComputationMemoryBudget.hpp`,
   `src/memory/ComputationMemoryBudget.cpp`, and
   `tests/memory/MemoryPlanTest.cpp`.
4. D1: configuration key, CLI option, help, example, validation.
5. D2: budget resolution.
6. D3/D4: formulas and `MemoryPlan`.
7. D5: `BinarySplitter::planFrontier` and its use in `splitParallel`.
8. D6/D7: guards, `MemoryAdmission`, and `main` check points.
9. Tests (below); add sources to `PI_ENGINE_SOURCES` and a `memory-plan-test`
   target with CTest name `MemoryPlanTest`; build and run CTest with
   `max(1, floor(nproc * 0.60))` jobs.
10. D8 calibration.
11. Update README, `config.toml.example`, CHECKLIST, CHANGELOG, and a new ADR
    in DECISIONS.

### Tests

- `MemoryPlanTest`:
  - `nodeBytes(start, end)` is at least the actual
    `8 * (|size(P)| + |size(Q)| + |size(T)|)` of
    `BinarySplitter::splitSequential(start, end)` for every
    `n in {1, 2, 31, 32, 1000, 10000}` and `start in {0, 1, 1000000}`.
  - Hand-computed exact values of `limbBytes`, `decimalIntegerBytes`, and
    `nodeBytes` for fixed inputs.
  - `resolve`: automatic reserve at 4 GiB, 16 GiB, 31.4 GiB, and 64 GiB;
    explicit budget returned unchanged; explicit budget above detected throws.
  - `planFrontier` equals the layout `splitParallel` executes, with and
    without seeds.
  - `enforce` reports the first exceeding stage in D4 order and the exact
    D6 message.
- `CommandLineTest`: `--memory-budget-bytes` parsing, malformed value as a
  usage error, help line and default present.
- `CommandLineCliTest`: `--digits 1000000 --memory-budget-bytes 1048576`
  exits 1, stderr contains `memory budget exceeded: stage=leaf_split`, and the
  working directory has no `pi.txt`, `checkpoints/`, `checkpoint-quarantine/`,
  or `storage/`; a budget above detected memory exits 1 with the D2 message.
- Manual: `--digits 50000000000` on this host exits 1 within 1 second.

### Out of Scope

- Changing Binary Splitting, merge, checkpoint, finalization, output, or
  verification algorithms (PR-0043 through PR-0047).
- Making 50,000,000,000 digits complete on a 30 GiB host.
- Runtime RSS polling, huge pages, NUMA.

### Acceptance

- An over-budget run exits 1 before creating output, storage, or checkpoint
  files, with the exact D6 message.
- No SIGKILL, abort, or core dump in the over-budget regression.
- D8 calibration holds at all three sizes.
- Existing results, checkpoint format, and all existing tests are unchanged.

### Next Contributor TODO

Begin after PR-0041 is complete. Start with step 1; new files require a
user-run creation script first.

## Future Plan: Bounded-Memory Large Calculation (PR-0043 to PR-0047)

### Status

Recorded on 2026-11-16 for reference only. Not approved for implementation.
Re-scope with PR-0042 measurements and get user approval before each PR.

### PR-0043: Streaming Checkpoint Serialization

Write header and P/Q/T magnitudes directly to the temporary block file with
incremental CRC32C, removing the payload-sized `std::vector`. Keep the
version-1 byte format, atomic rename, and existing-manifest preservation on
failure.

### PR-0044: Memory-Bounded Leaf Computation

Size leaf ranges by estimated working set instead of `workers * 4`. Gate leaf
tasks with a byte-permit admission controller decoupled from worker count.
Persist and release each completed leaf immediately; extend the frontier
writer to publish non-overlapping partial ranges so resume recalculates only
the complement. Publish `termsCompleted` per leaf.

### PR-0045: Pair-at-a-Time External Merge

Keep the frontier as metadata. For each admitted pair: reload only its two
operands, merge with GMP, durably write the parent, replace the manifest,
then delete children. Concurrency is bounded by byte permits. Level barriers,
adjacency, and result order stay unchanged.

### PR-0046: Bounded Finalization and Streaming Output

Remove `PiCalculationResult::decimal` and stream decimal output to a temporary
file with atomic rename. Replace the `10^(2W)` square-root construction with a
result-sized scaled integer square root using GMP only, preserving floor,
guard-digit, and rounding contracts.

### PR-0047: Verification Memory Planning

Include BBP decimal import numerator, denominator, and per-sample scratch in
the memory plan and reject over-budget verification before it starts. Keep
known-digits and SHA-256 streaming. Do not weaken or skip BBP without a
proven exact bounded-memory replacement and benchmark.

### Shared Verification Requirements

- Peak RSS, throughput, storage use, and total time before and after each PR
  at fixed workloads, budgets, worker counts, and compression settings.
- Kill-and-resume at every durable boundary matches an uninterrupted run's
  SHA-256.
- Version-1 checkpoints from earlier releases still resume.
