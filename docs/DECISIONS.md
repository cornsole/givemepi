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

PR-0020 defines the versioned P/Q/T block and atomic commit format with checksum
metadata from the beginning.

PR-0021 implements structural, checksum, manifest, and modular verification and
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
