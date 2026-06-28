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