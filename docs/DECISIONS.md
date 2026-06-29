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