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