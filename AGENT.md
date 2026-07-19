# AGENT.md

Session Start Procedure

When starting a new implementation session, always read the following files in order:


1. AGENT.md
2. docs/DECISIONS.md
3. docs/ROADMAP.md
4. docs/IMPLEMENTATION_PLAN.md
5. docs/CHECKLIST.md
6. docs/CHANGELOG.md
7. Current source files

The following md files are in the docs folder

Do not begin implementation until the current project state is understood.

------------------------------------------------

Session End Procedure

Before ending a session:

1. Verify code builds.
2. Update docs/CHECKLIST.md.
3. Update docs/CHANGELOG.md.
4. Update docs/DECISIONS.md if architecture changed.
5. Update docs/ROADMAP.md if priorities changed.
6. Update docs/IMPLEMENTATION_PLAN.md if an approved plan or PR boundary changed.
7. Leave clear TODOs for the next contributor in docs/IMPLEMENTATION_PLAN.md.

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

IMPLEMENTATION_PLAN.md

If an approved implementation sequence, PR boundary, or future extension plan
changes, update the implementation plan after user confirmation.

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

✓ Commit created

✓ Commit pushed

---

# Commit / Push Rule

When a PR is completed:

1. Verify build.
2. Verify tests.
3. Update documentation.
4. Create a git commit.
5. Push the commit.

PR completion requires the commit to exist remotely.

Commit message format:

PR-ID: short description

Example:

PR-0004: Add memory layer

Rules:

- One completed PR must have one corresponding commit.
- Do not combine multiple PR completions into a single commit.
- Do not mark a PR complete before commit and push.
- Keep commit history aligned with CHANGELOG history.

# AI Collaboration Rules

## Planning Document Rule

Agent-authored implementation plans belong in:

docs/IMPLEMENTATION_PLAN.md

Before recording a new plan or changing an existing plan:

1. Present the proposed plan to the user.
2. Wait for explicit user approval.
3. Record only the approved scope.

Never store agent-authored plans in `PR-progress/`.

## PR-progress Ownership Rule

The `PR-progress/` directory is reserved for the developer's own work records.

Agents may read these records when needed to understand current work, but must
not create, edit, append, delete, rename, move, reformat, or replace files in
`PR-progress/` unless the user explicitly requests that exact operation.

Do not use `PR-progress/` for:

- agent implementation plans
- future PR proposals
- generated checklists
- session summaries
- agent status tracking

When leaving agent TODOs or approved future plans, use
`docs/IMPLEMENTATION_PLAN.md`.

## PR Work Sequence Rule

At the start of every PR implementation:

Read the approved work sequence from `docs/IMPLEMENTATION_PLAN.md` before
modifying code.

If no approved sequence exists, propose one to the user and wait for approval
before recording it in `docs/IMPLEMENTATION_PLAN.md`.

Example:

```
1. File check
   include/bigint/GMPInteger.hpp
        |
        v

2. File check
   src/bigint/GMPInteger.cpp
        |
        v

3. Big Integer API design
        |
        v

4. Add required operations
        |
        v

5. Extend GMPIntegerTest
        |
        v

6. Check CMake
```

The sequence must reflect the actual implementation order.

Do not start implementation before the affected files and modification order are identified.

## Modification Safety Rule

When modifying existing files:

Prefer partial modifications over full file replacement.

Do not replace the entire file unless:

* The file is newly created.
* The file has no existing implementation.
* Full replacement is explicitly required.

Partial modifications must always specify:

* File path
* Existing content
* Replacement content
* Modification location

## Commit Message Rule

During normal development:

Do not create commit commands unless requested.

Only provide the recommended commit message.

The developer decides when and how to commit.

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

## Development Instruction Rule

When modifying existing files, provide exact modification instructions.

Required format:

Existing:

```text
original content
```

Replace:

```text
new content
```

The developer must be able to apply changes without guessing.

For partial modifications, always specify:

* file path
* exact location
* existing code
* replacement code

## Command Execution Rule

Whenever a command must be executed, specify the required working directory.

Every command instruction must include:

* execution path
* command
* expected result

Example:

Project root:

```bash
cd /path/to/project
```

Run:

```bash
cmake -S . -B build
cmake --build build
```

Do not provide commands without telling where they should be executed.

--------------------------------------------------


## Changelog Versioning Rule

Every completed PR must update `docs/CHANGELOG.md`.

Version format:

```md
## [VERSION] - PR-ID
```

Rules:

* Each completed PR gets a unique version entry.
* PR version must be recorded before marking the PR as completed.
* Do not rewrite previous changelog history.
* New PR entries are appended above previous entries.
* Keep existing entries unchanged unless fixing an actual mistake.

Version progression:

### Patch version

Use when:

* Documentation changes
* Build fixes
* Bug fixes
* Small feature additions

Example:

* PR-0002-1 → [0.1.0]
* PR-0002-2 → [0.1.1]

### Minor version

Use when:

* New subsystem added
* Architecture expanded
* Major feature completed

Example:

* PR-0003 → [0.2.0]

### Major version

Use when:

* Public API redesign
* Project architecture rewrite
* Breaking changes

The changelog entry must include:

### Added

New files, modules, features.

### Changed

Modified behavior, architecture, build system.

### Fixed

Bug fixes or corrected implementation issues.

Never mark a PR complete while CHANGELOG.md is outdated.

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
