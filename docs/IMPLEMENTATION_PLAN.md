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
| PR-0018 | Chudnovsky leaf and range validation | Planned |
| PR-0019 | Cutoff-based parallel Binary Splitting | Planned |
| PR-0020 | P/Q/T checkpoint block foundation | Planned |
| PR-0021 | Checkpoint integrity verification | Planned |
| PR-0022 | Progress snapshot and reporting | Planned |

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

### Scope

- validate `[start, end)` input ranges
- implement Chudnovsky leaf P/Q/T values
- test known leaf and small-range P/Q/T results
- add a small known-digits integration test
- preserve the existing merge equation and sequential API

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

---

## PR-0020: P/Q/T Checkpoint Block Foundation

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

### Boundary

Worker threads never perform checkpoint disk I/O. They publish completed block
data to the checkpoint writer.

The presence of a block file does not make it reusable. PR-0021 performs all
validation before resume accepts it.

### Definition of Done

- A completed P/Q/T range round-trips without value loss.
- Partial files never receive a final checkpoint name.
- The header can be extended through format versioning.
- Checksum metadata is part of the initial format.
- Existing computation paths remain independent from storage details.

---

## PR-0021: Checkpoint Integrity Verification

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

## PR-0022: Progress Snapshot and Reporting

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
- resume reconstructs counters from PR-0021 validated blocks

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
