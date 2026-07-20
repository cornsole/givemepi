# DECISIONS.md

This document records all architectural decisions.

Rules

- Never rewrite history.
- Never delete previous decisions.
- Decisions are append-only.
- If a decision changes, create a new record explaining why.

---

## ADR-0001

Date

2026-06-28

Status

Accepted

Title

Use Chudnovsky Formula

Decision

Use the Chudnovsky Formula as the primary π algorithm.

Reason

Currently one of the fastest known series for π.

Alternatives

- Gauss-Legendre
- Borwein
- Ramanujan

Consequence

Requires Binary Splitting.

---

## ADR-0002

Status

Accepted

Title

Use Binary Splitting

Decision

Use Binary Splitting instead of direct summation.

Reason

Significantly reduces complexity.

Consequence

Tree-based scheduler required.

---

## ADR-0003

Status

Accepted

Title

Use GMP

Decision

Delegate Big Integer multiplication to GMP.

Reason

GMP already implements

- Karatsuba
- Toom Cook
- FFT

Better than maintaining custom implementation.

---

## ADR-0004

Status

Accepted

Title

No OS Swap

Decision

Do not rely on operating system swap.

Reason

Swap is unpredictable.

Implement dedicated Out-of-Core Storage Layer.

Consequence

Need Storage Manager.

---

## ADR-0005

Status

Accepted

Title

Checkpoint System

Decision

Support resumable checkpoints.

Reason

Large computations may run for days.

Consequence

Checkpoint Manager required.

---

## ADR-0006

Status

Accepted

Title

SIMD Scope

Decision

Use SIMD only for memory operations.

Reason

GMP already provides optimized arithmetic.

Consequence

SIMD limited to

- memcpy

- limb operations

- prefetch

- compare

---

## ADR-0007

Status

Accepted

Title

Documentation First

Decision

Implementation is incomplete until documentation is updated.

Reason

Multiple AI agents participate.

Documentation synchronization prevents duplicated work.

Consequence

Every completed task updates

CHECKLIST.md

CHANGELOG.md

DECISIONS.md (if required)

ROADMAP.md (if required)

---

## ADR-0008

Date

2026-06-29

Status

Accepted

Title

Use toml++ for Configuration Parsing

Decision

Use toml++ as the TOML configuration parser.

Reason

The project requires a lightweight configuration system with:

- Human readable configuration files
- Type-safe value loading
- Header-only integration
- Simple dependency management

Alternatives

- Manual parser implementation
- Other TOML libraries

Consequence

ConfigLoader depends on toml++.

Third-party dependency is managed through:

scripts/install-tomlpp.sh

---

## ADR-0009

Date

2026-06-29

Status

Accepted

Title

Configuration Priority Order

Decision

Configuration values are applied in this order:

Default Values

↓

config.toml

↓

Command Line Arguments

Reason

Provides safe defaults while allowing persistent configuration and runtime overrides.

Consequence

ConfigLoader and CommandLine must preserve this priority order.

---

## ADR-0010

Date

2026-06-29

Status

Accepted

Title

PR Based Changelog Versioning

Decision

Every completed PR must create a version entry in CHANGELOG.md.

Format:

## [VERSION] - PR-ID

Reason

Multiple agents work on the project.

Versioned changelog entries improve:

- Change tracking
- Release management
- Collaboration

Consequence

PR completion requires documentation updates.

---

### ADR-0011

Runtime CPU Feature Detection

---

## ADR-0012

Date

2026-06-29

Status

Accepted

Title

Memory Layer Design

Decision

Use dedicated memory management components instead of relying only on general heap allocation.

Components:

- Arena Allocator
- Memory Pool
- Alignment Manager
- Scratch Buffer


Reason

Large π computations require predictable memory allocation patterns.

Frequent heap allocations can cause:

- fragmentation
- allocation overhead
- cache inefficiency


Consequence

Future computation modules should use the memory layer for temporary and high-frequency allocations.

Binary Splitting and Big Integer integration will use these components where appropriate.

---

## ADR-0013

Date

2026-06-29

Status

Accepted

Title

Template Implementation Separation

Decision

Template-based scheduler components keep declarations in header files and implementations in separate .inl files.

Reason

Scheduler components such as Lock-Free Queue require templates while maintaining readable interfaces.

Keeping all implementation inside headers increases file size and reduces maintainability.

Consequence

Template scheduler components will use:

include/scheduler/*.hpp
include/scheduler/*.inl

instead of placing implementation in .cpp files.

---

## ADR-0014

Date

2026-06-29

Status

Accepted

Title

Reference Queue Before Lock-Free Queue

Decision

Implement the Scheduler using a reference queue implementation before introducing a lock-free queue.

The reference queue will use standard library synchronization primitives and keep the Scheduler API independent from the queue implementation.

The lock-free queue will replace the reference implementation in a later development phase without changing the Scheduler or Worker interfaces.

Reason

The primary goal of the current phase is to validate the scheduler architecture.

A production-quality lock-free MPMC queue is significantly more complex and would increase the scope of the current PR.

Separating architecture verification from performance optimization keeps each PR small, independently buildable, and fully testable.

This decision supersedes ADR-0013 for the current Scheduler implementation phase.

Alternatives

- Implement the lock-free queue immediately.
- Delay Scheduler implementation until the lock-free queue is complete.

Consequence

Scheduler development proceeds using a reference queue implementation.

Lock-free queue development becomes an independent implementation phase.

Existing Scheduler tests will be reused to verify correctness after replacing the queue implementation.

--

## ADR-0015

Date

2026-06-29

Status

Accepted

Title

Retain Reference Queue for Validation

Decision

Keep the Reference Queue implementation alongside the Lock-Free Queue during development.

The Scheduler will depend on an IQueue interface rather than a concrete queue implementation.

Reason

Maintaining a reference implementation allows:

- Correctness verification
- Regression testing
- Performance benchmarking against a known baseline
- Easier debugging of lock-free behavior

Alternatives

- Replace the Reference Queue immediately after implementing the Lock-Free Queue

Consequence

The project will maintain two queue implementations during development:

- ReferenceQueue
- LockFreeQueue

Production Scheduler code will depend only on the IQueue abstraction.

---

## ADR-0016

Date

2026-06-30

Status

Accepted

Title

Scheduler Queue Abstraction

Decision

Scheduler uses a queue abstraction layer.

The Scheduler and Worker components do not depend on a specific queue implementation.

Queue implementations provide the same interface and can be replaced independently.

Current implementations:

- ReferenceQueue
- LockFreeQueue


Reason

Scheduler architecture needs to be validated independently from queue performance optimization.

A reference implementation allows correctness testing before introducing complex concurrent data structures.

The lock-free queue can be evaluated and replaced without modifying Scheduler or Worker.


Alternatives

- Couple Scheduler directly with LockFreeQueue.
- Delay Scheduler implementation until LockFreeQueue is production-ready.


Consequence

Scheduler development can continue with interchangeable queue backends.

Future queue optimization will not require Scheduler architecture changes.

---

## ADR-0017

Date

2026-06-30

Status

Accepted

Title

ThreadPool Owns Worker Lifecycle

Decision

ThreadPool manages Worker creation, execution, and shutdown.

Scheduler delegates worker management responsibilities to ThreadPool.

Reason

Scheduler should coordinate task execution rather than directly manage worker threads.

Separating responsibilities keeps queue abstraction, worker management, and scheduling logic independent.

Alternatives

- Keep Worker management inside Scheduler.
- Create threads directly inside Scheduler.

Consequence

ThreadPool becomes the owner of:

- Worker instances
- Worker lifecycle
- Task submission routing

Scheduler only controls high-level scheduling operations.

---

## ADR-0018

Date

2026-06-30

Status

Accepted

Title

Stage Parallel Binary Splitting Implementation

Decision

Implement Binary Splitting parallelization in two stages.

Stage 1 introduces a parallel-ready API while preserving the current sequential implementation.

Stage 2 integrates Scheduler synchronization primitives to execute recursive Binary Splitting tasks in parallel.

Reason

The current Scheduler executes fire-and-forget tasks but does not provide task synchronization or result aggregation.

Binary Splitting requires joining recursive computations before merging intermediate results.

Separating API preparation from Scheduler synchronization keeps each PR independently buildable, testable, and easier to review.

Alternatives

- Implement Parallel Merge immediately with temporary synchronization.
- Delay all Binary Splitting changes until Scheduler synchronization is complete.

Consequence

BinarySplitter will expose both sequential and parallel-ready execution paths.

Actual parallel execution will be implemented after Scheduler gains task synchronization support (Future/TaskGroup/Join mechanism).

This keeps the BinarySplitter interface stable while allowing Scheduler improvements to be integrated later.

---

## ADR-0019

Date

2026-07-19

Status

Accepted

Title

Shared Task Completion State and Exception Propagation

Decision

`Task` and its returned `TaskHandle` share one reference-counted task state.

The shared state owns:

- `TaskState`
- captured `std::exception_ptr`
- completion mutex
- completion condition variable

`TaskHandle` observes the shared state and does not own or reference the `Task`
object itself. The handle remains move-only.

`ThreadPool::submit(Task)` and `Scheduler::submit(Task)` return a `TaskHandle`.
Accepted tasks return a valid handle. Rejected submissions return an invalid
handle.

`Task::execute()` captures task exceptions, marks the shared state as failed,
and notifies waiters. Exceptions must not escape the worker thread. Callers
inspect or rethrow captured failures through the handle.

Reason

Binary Splitting requires callers to wait for child tasks before merging their
P/Q/T results. The current fire-and-forget scheduler cannot express this join.

Embedding independent synchronization members in both `Task` and `TaskHandle`
would not provide one observable completion state. Holding a
`std::shared_ptr<Task>` in the handle would also couple task lifetime and result
observation unnecessarily.

Alternatives

- Keep fire-and-forget boolean submission.
- Store a `std::shared_ptr<Task>` in each handle.
- Give `Task` and `TaskHandle` separate atomic states and synchronization data.
- Introduce Binary Splitting-specific synchronization outside the scheduler.

Consequence

Scheduler callers can wait for accepted work and observe completion or failure
without polling.

Worker threads remain alive when an individual task throws.

PR-0016 implements this shared-state synchronization contract without changing
the existing queue topology.

Scheduler shutdown semantics and global/local queue routing are handled in
PR-0017 before parallel Binary Splitting is introduced.

---

## ADR-0020

Date

2026-07-19

Status

Accepted

Title

Layered Checkpoint and Result Integrity Verification

Decision

Checkpoint integrity and mathematical result verification are separate layers.

Every checkpoint block uses a versioned header containing:

- file magic
- format version
- target computation identity
- Chudnovsky range `[a, b)`
- tree level
- P/Q/T payload lengths
- checksum type and checksum value
- completion metadata

Checkpoint files are written to a temporary path, flushed, synchronized, and
atomically renamed only after the complete payload and checksum are available.

Resume accepts only blocks that pass all applicable checks:

1. header and format validation
2. range, level, and payload-size validation
3. payload checksum validation
4. manifest consistency validation
5. independent modular verification of P/Q/T values

Incomplete temporary files and invalid blocks are never treated as completed
work. They are quarantined or ignored and their ranges are scheduled for
recalculation.

Final output verification remains a separate stage using output hashing, known
digits, and BBP spot checks.

Reason

A file checksum detects storage corruption but does not prove that a validly
stored P/Q/T value was computed correctly. Independent modular verification is
required to detect computational errors without repeating full-precision work.

Resume correctness depends on trusting only validated blocks. Persisted
progress counters alone are not sufficient evidence that work is reusable.

Alternatives

- Trust every file present in the checkpoint directory.
- Use only a payload checksum.
- Store only a manifest completion flag.
- Recompute every checkpoint block at startup.

Consequence

PR-0021 defines the versioned P/Q/T block and atomic commit format with checksum
metadata from the beginning.

PR-0022 implements structural, checksum, manifest, and modular verification and
resumes only from validated blocks.

Progress reconstruction uses the set of validated blocks rather than stored
percentage values.

---

## ADR-0021

Date

2026-07-19

Status

Accepted

Title

Decouple Progress Tracking from CLI Reporting

Decision

Computation and scheduler code do not print progress directly.

A thread-safe `ProgressTracker` records raw counters and produces immutable
`ProgressSnapshot` values. A reporter interface consumes snapshots without
owning or controlling computation.

The snapshot model is versioned and can expose:

- current computation phase
- target digits and total terms
- completed terms and blocks
- current merge level
- active and queued tasks
- elapsed time, throughput, and optional ETA
- current memory use
- checkpoint bytes
- last validated checkpoint
- terminal completion or failure state

The initial reporters are:

- human-readable CLI text
- machine-readable CLI JSON

TTY output may update one terminal line. Non-TTY output emits complete records
that remain suitable for logs and automation. Future status-file, GUI, metrics,
or remote reporters implement the same reporter boundary.

Worker threads update lightweight counters only. Formatting and output run on a
dedicated reporting path and must not perform disk I/O on computation workers.

TOML and command-line configuration control whether reporting is enabled, the
reporting interval, and the output format. One shared validation path runs after
defaults, TOML loading, and command-line overrides have been applied.

On resume, progress is reconstructed from checkpoint blocks that passed ADR-0020
integrity validation. A persisted percentage is never treated as authoritative.

Reason

Direct CLI output from algorithms would couple computation to one presentation
environment and make JSON, status files, GUI integration, and automated
monitoring difficult to add later.

Raw counters remain useful even when percentage and ETA estimates change as the
cost of high-level merges grows.

Alternatives

- Print directly from worker tasks.
- Store a single global percentage value.
- Make the CLI query internal scheduler containers directly.
- Persist and trust only the last displayed percentage.

Consequence

PR-0022 introduces the phase model, tracker, snapshot, configuration controls,
and CLI text/JSON reporters.

Additional reporting environments can be added without changing Binary
Splitting, checkpoint, or scheduler implementations.

---

## ADR-0022

Date

2026-07-19

Status

Accepted

Title

Explicit Restartable Scheduler Lifecycle

Decision

`Scheduler` and `ThreadPool` use one observable lifecycle model:

- `Stopped`
- `Running`
- `Stopping`

A newly constructed scheduler is `Stopped`. `start()` moves it to `Running`,
and `stop()` moves it through `Stopping` to `Stopped`. Repeated start or stop
calls are idempotent in their corresponding stable state.

Calls are serialized by a lifecycle mutex. A start that overlaps `Stopping`
waits for that stop operation and then restarts the workers. A stop that
overlaps another stop waits for the same stop generation to complete.

Submission is accepted only while the state is `Running`. Once `Stopping` is
published, new submissions return an invalid `TaskHandle`. `running()` returns
true only for `Running`; callers that need the full distinction use `state()`.

The lifecycle mutex is released before joining workers. This prevents a worker
task attempting submission during shutdown from waiting on a lock held by the
thread that is joining that same worker.

Restart after a completed stop is supported.

Reason

The previous atomic boolean did not distinguish a completed stop from a stop in
progress and did not serialize start, stop, and submission. That allowed races
where work could be accepted after shutdown began or worker threads could be
started and stopped concurrently.

A distinct `Stopping` state creates a clear submission cutoff and gives drain
shutdown a stable lifecycle boundary.

Alternatives

- Keep one atomic running boolean.
- Make Scheduler instances single-use after stop.
- Hold the lifecycle mutex while joining every worker.
- Allow submission during `Stopping`.

Consequence

Scheduler lifecycle transitions are observable and restartable. Concurrent
start, stop, and submit calls have a defined ordering.

The drain guarantee paired with this lifecycle cutoff is defined and completed
by ADR-0023. This decision defines the cutoff; ADR-0023 defines completion of
work accepted before it.

---

## ADR-0023

Date

2026-07-20

Status

Accepted

Title

Outstanding-Task Drain and Context-Aware Queue Routing

Decision

`ThreadPool` maintains an atomic count of accepted tasks that have not finished
execution. The count includes tasks in the global queue, tasks in worker-local
queues, stolen tasks, and tasks currently executing.

The task count is reserved before a task is published to a queue. A rejected or
throwing queue insertion rolls back the reservation. A worker decrements the
count only after `Task::execute()` has published completion or failure.

Shutdown uses two phases:

1. Publish the stop request to every worker.
2. Join every worker after it drains accepted work.

A worker continues local pop, global pop, and steal attempts after receiving a
stop request. It exits only when the accepted outstanding-task count is zero.
Therefore `stop()` returns only after every accepted `TaskHandle` is terminal.

Submission routing depends on the calling context:

- external thread to bounded global `IQueue`
- worker owned by this pool to that worker's local queue
- worker owned by another pool to this pool's global queue

The public API remains `submit(Task) -> TaskHandle`. A full global queue rejects
the external submission with an invalid handle.

Reason

Queue emptiness alone cannot prove that drain is complete. A queue may be empty
while a worker is still executing an accepted task, and separately checking
multiple local queues creates transient snapshots with no single correctness
boundary.

Counting accepted unfinished work creates one drain invariant across global,
local, stolen, and active tasks.

External submissions must use the global queue so configured capacity and
rejection are observable. Worker-created child work belongs in the local queue
so it remains cheap and available for stealing.

Alternatives

- Stop workers immediately when a stop flag is set.
- Exit when every queue appears empty.
- Route every submission directly to a round-robin worker local queue.
- Expose separate public APIs for external and local submission.
- Join each worker immediately after requesting that individual worker stop.

Consequence

Accepted work cannot remain permanently pending after normal drain shutdown.
Global queue capacity applies to external producers, while nested worker work
uses local queues without changing the scheduler API.

The current idle path still yields while waiting for work or drain completion.
Wake-up optimization remains deferred until benchmarked.

---

## ADR-0024

Date

2026-07-20

Status

Accepted

Title

Bounded MPMC Slot-Sequence Contract

Decision

`LockFreeQueue` remains a bounded multi-producer, multi-consumer queue with one
atomic sequence value per slot and separate producer and consumer position
counters.

Capacity must be at least two and no greater than `PTRDIFF_MAX`. This keeps the
occupied and reusable slot generations distinct and makes modular sequence
ordering unambiguous within the supported queue distance.

Push and pop classify a slot sequence relative to the operation's expected
sequence:

- equal: claim the position with compare-and-exchange
- behind: report full to push or empty to pop
- ahead: reload the shared position because the prior observation is stale

The position counters use relaxed atomic operations because they only allocate
positions. A release store to the slot sequence publishes the task payload, and
an acquire load of that sequence makes the payload visible before it is moved.
The same release/acquire relationship protects reuse after consumption.

`empty()` inspects the next consumer slot sequence. Its answer is only a
concurrent snapshot and cannot replace accepted outstanding-task accounting for
drain shutdown.

Reason

Treating every sequence mismatch as full or empty confuses contention with a
stable queue boundary. Another producer or consumer can advance the shared
position between the initial position load and the slot sequence load, leaving
the observer with a stale position even when the queue can still make progress.

Capacity zero causes invalid slot indexing. With one slot, the initial free
sequence and the post-consumption reusable sequence collide with the occupied
generation required by this protocol.

Alternatives

- Serialize the queue with a mutex.
- Keep retrying every mismatch without distinguishing true full or empty.
- Require a power-of-two capacity and mask slot indices.
- Use reservation-counter equality as the definition of empty.

Consequence

Queue contention no longer creates false rejection solely from a stale
position. Arbitrary capacities of at least two remain supported, and slot
publication has an explicit memory-ordering contract.

Direct tests validate capacity boundaries, FIFO and slot reuse, unused-capacity
producer contention, and exactly-once execution under concurrent producers and
consumers.

---

## ADR-0025

Date

2026-07-20

Status

Accepted

Title

Use a Staged DAG for Parallel Binary Splitting

Decision

Parallel Binary Splitting uses a caller-orchestrated staged DAG rather than
recursive worker tasks that submit child tasks and wait for their handles.

The caller partitions the input into balanced sequential leaf blocks, bounded
by an explicit tasks-per-worker policy. Tasks write exclusively owned result
slots and never wait for other tasks. After joining one complete level, the
caller submits adjacent P/Q/T merges for the next level. Levels with fewer than
two merge pairs execute inline.

The parallel API receives a scheduler reference and an explicit cutoff. It
falls back synchronously for small ranges, stopped or zero-worker schedulers,
calls from a worker owned by that scheduler, and rejected submissions.

Reason

The scheduler's `TaskHandle::wait()` blocks but does not execute queued work.
Recursive fork/join can therefore occupy every worker with a parent waiting on
children that remain in local queues. A staged DAG has no worker-side waits and
makes every dependency boundary explicit.

Bounded independent blocks limit task metadata, expose useful parallelism, and
map naturally to future checkpoint, progress, NUMA, and memory-reclamation
stages. Internal result slots avoid expanding the completion-only task handle
into a generic future before other consumers require that abstraction.

Alternatives

- Submit recursive child tasks and block worker threads on their handles.
- Add cooperative wait-and-help behavior to the scheduler in the same PR.
- Introduce a generic typed future and task graph runtime before parallelizing.
- Use a global scheduler and a hidden fixed cutoff.

Consequence

Parallel execution cannot deadlock from every worker waiting on queued child
work. Sequential and fallback paths retain exact P/Q/T equality, and task count
is bounded by worker count rather than total term count.

The caller synchronizes between merge levels, and current-worker calls do not
parallelize. Cooperative scheduler waits and a generic task DAG remain possible
future extensions if broader workloads justify their complexity with
benchmarks.

---

## ADR-0026

Date

2026-07-20

Status

Accepted

Title

Complete the End-to-End Calculation Before Checkpoint Storage

Decision

Insert an end-to-end Chudnovsky calculation PR before the checkpoint block
foundation. The new PR-0020 defines digit-to-term conversion, guard precision,
integer fixed-point finalization, normalized decimal output, and end-to-end
benchmarks. The previously planned checkpoint, integrity, and progress work
moves to PR-0021, PR-0022, and PR-0023 respectively.

Final pi construction uses guarded GMP integer arithmetic and integer square
root instead of promoting the test-only `mpf_t` approximation into the
production API.

Reason

Checkpoint identity and compatibility depend on the requested digits, guard
policy, term count, arithmetic version, and finalization contract. Defining a
durable format before those fields exist risks format churn and makes it
impossible to benchmark the real end-to-end bottleneck.

Integer fixed-point finalization makes rounding inputs explicit, avoids hidden
global floating-point precision, and preserves the project's GMP integer
boundary for very large calculations.

Alternatives

- Implement checkpoint blocks before the final calculation contract.
- Promote the test-only GMP floating-point calculation directly to production.
- Combine finalization, checkpoint, resume, and progress in one PR.

Consequence

The next implementation target is PR-0020 end-to-end Chudnovsky calculation.
Checkpoint work starts only after computation identity fields and realistic
performance measurements are available. Existing checkpoint architecture
decisions remain valid but their planned PR numbers move forward by one.

---

## ADR-0027

Date

2026-07-20

Status

Accepted

Title

Use Canonical Versioned Checkpoint Bytes and Atomic Synchronous Storage

Decision

Checkpoint compatibility is based on the Chudnovsky algorithm and formula
versions, requested and guarded precision, working digits, and term count.
Runtime worker, cutoff, queue, and timing settings are separate provenance.

P/Q/T values use canonical signed magnitude with minimal big-endian bytes.
Blocks and manifests encode every field explicitly, protect canonical bytes
with CRC32C, and reject non-canonical or inconsistent input. Storage uses a
unique same-directory temp file, complete writes, file synchronization, atomic
rename, and parent-directory synchronization.

Reason

Portable canonical bytes prevent compiler ABI, padding, native endianness, and
equivalent-encoding differences from changing durable checkpoint identity.
Separating codec and synchronous storage permits a future writer thread or
out-of-core layer without changing the file contract or adding disk I/O to
scheduler workers.

Consequence

Equivalent blocks and manifests produce deterministic bytes and paths across
execution policies. Failures before rename preserve the previous final file.
PR-0022 remains responsible for deciding whether persisted blocks are trusted
for resume, including manifest-set consistency and independent modular P/Q/T
verification.

---

## ADR-0028

Date

2026-07-20

Status

Accepted

Title

Verify Canonical Final Output Through Independent Evidence Layers

Decision

Final decimal output is canonical `3.` plus exactly the requested ASCII
fractional digits. Stored files add one LF byte, while SHA-256 covers only the
newline-free canonical bytes.

Every completed CLI computation uses three independent evidence layers:

- a versioned rounded known-decimal prefix;
- canonical SHA-256, persisted in a versioned CRC32C-protected manifest;
- deterministic BBP hexadecimal samples compared with hexadecimal digits
  extracted from the actual decimal output through exact GMP arithmetic.

BBP calculation retains a numeric error bound and returns inconclusive at a
digit boundary. Decimal extraction includes the complete half-decimal-ULP
rounding interval. A skipped inapplicable check is neutral when other required
checks pass, but an all-skipped report remains skipped.

Final verification manifests are published only for passed reports through
durable same-directory atomic replacement. Later file validation compares the
stored canonical SHA-256 so arbitrary middle and trailing digit mutation is
detectable independently of sampled mathematical checks.

Reason

Hashing, known digits, and BBP detect different failure classes. BBP must check
the generated output rather than only another embedded reference to remain an
independent mathematical cross-check. Canonical bytes and a versioned manifest
make verification portable and reproducible outside the calculation process.

Consequence

Progress schema version 2 includes `verifying_output`. The CLI owns output,
verification, manifest publication, and terminal progress after the calculator
finishes arithmetic. Default verification uses eight deterministic samples;
future large-scale latency work should parallelize independent BBP samples
without creating threads inside the algorithm.
