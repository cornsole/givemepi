# AGENT.md

Session Start Procedure

When starting a new implementation session, always read the following files in order:


1. AGENT.md
2. DECISIONS.md
3. ROADMAP.md
4. CHECKLIST.md
5. CHANGELOG.md
6. Current source files

The following md files are in the docs folder

Do not begin implementation until the current project state is understood.

------------------------------------------------

Session End Procedure

Before ending a session:

1. Verify code builds.
2. Update CHECKLIST.md.
3. Update CHANGELOG.md.
4. Update DECISIONS.md if architecture changed.
5. Update ROADMAP.md if priorities changed.
6. Leave clear TODOs for the next contributor.

# Project

High Performance Pi Calculator

Goal

Create one of the fastest CPU-based π calculators written in modern C++20.

The project targets billions of digits while keeping the code maintainable.

---

# Development Philosophy

Correctness

↓

Stability

↓

Maintainability

↓

Performance

↓

Micro Optimization

Never sacrifice correctness for speed.

---

# Core Algorithms

Mandatory

- Chudnovsky Formula
- Binary Splitting
- GMP Big Integer
- GMP FFT Multiplication

Do not replace core algorithms unless benchmarked.

---

# Threading

Always use

Thread Pool

↓

Lock-Free Queue

↓

Work Stealing

Never create detached threads.

Never create threads inside algorithms.

---

# Memory

Always use

Arena Allocator

Memory Pool

Huge Pages when available.

Avoid heap fragmentation.

---

# SIMD

Allowed

AVX2

AVX512

Never duplicate GMP functionality.

---

# Assembly

Assembly is allowed only for

- CPUID
- memcpy
- Prefetch
- rdtsc
- cache instructions
- spin wait

Never implement Big Integer multiplication.

---

# Storage

Never rely on OS swap.

Always use Out-of-Core Storage Layer.

---

# Checkpoint

Checkpoint must always support Resume.

Checkpoint format must remain backward compatible.

---

# Verification

Every completed computation must pass

- Hash Verification
- BBP Spot Check
- Known Digits

---

# Documentation Rules

Every public API must be documented.

Every algorithm must describe

- Purpose
- Input
- Output
- Time Complexity
- Memory Complexity

---

# Benchmark Rules

Every optimization requires benchmark.

No benchmark

↓

No optimization

---

# Required Document Maintenance

After every completed task the following documents MUST be updated.

CHECKLIST.md

- Mark completed tasks.
- Add newly discovered tasks if needed.

CHANGELOG.md

- Record every implemented feature.
- Record refactoring.
- Record bug fixes.

DECISIONS.md

If architectural decisions changed

OR

new important decisions were made

append a new decision entry.

ROADMAP.md

If implementation order changes

update roadmap.

Never leave documentation behind code.

Documentation is considered part of the implementation.

A task is NOT complete until documentation has also been updated.

---

# Pull Request Rule

Before considering a feature complete verify

✓ Code builds

✓ Tests pass

✓ Benchmark executed (if performance related)

✓ Documentation updated

✓ Checklist updated

✓ Changelog updated

✓ Decision log updated if required

# AI Collaboration Rules

## Directory / File Creation

If new directories or files are required:

DO NOT create them manually in explanations.

Instead:

1. Provide a shell script (bash) that creates every required directory and empty file.
2. The user executes the script.
3. Continue implementation afterwards.

--------------------------------------------------

## Source Files

For every source/header file:

Always provide

- File path
- File name
- Complete file contents

Never provide partial snippets unless explicitly requested.

--------------------------------------------------

## Documentation

Whenever implementation completes,

update

- CHECKLIST.md
- CHANGELOG.md
- DECISIONS.md (if required)
- ROADMAP.md (if required)

Documentation updates are part of the implementation.

--------------------------------------------------

## Implementation Size

Prefer small implementation units.

Each implementation should generally affect one subsystem.

Avoid giant commits.

--------------------------------------------------

## Build Safety

Every implementation should compile independently whenever practical.

Never intentionally leave broken intermediate states.

--------------------------------------------------

## File Changes

If modifying an existing file,

clearly specify

- file path
- what changes
- complete replacement OR patch

Never leave ambiguity.