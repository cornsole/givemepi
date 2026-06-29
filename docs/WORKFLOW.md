Project
    ↓
Foundation
    ↓
Platform
    ↓
Memory
    ↓
Scheduler
    ↓
Progress
    ↓
Big Integer
    ↓
Storage
    ↓
Checkpoint
    ↓
Binary Splitting
    ↓
Chudnovsky
    ↓
Verification
    ↓
SIMD
    ↓
Assembly
    ↓
Benchmark & Performance Tuning

---

# Development Execution Rules

## Command Execution Context

All commands must specify the execution directory.

Every command instruction must include:

- Working directory
- Command
- Expected result


Example:

Project root:

```bash
cd /path/to/project
```

Configure:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

Expected result:

* Build completes successfully.
* No compiler errors.
* No linker errors.

Do not provide commands without specifying where they should be executed.

---

## Build Verification Workflow

After modifying source code:

1. Move to project root.

```bash
cd /path/to/project
```

2. Configure.

```bash
cmake -S . -B build
```

3. Build.

```bash
cmake --build build
```

Verify:

* Binary generated
* Tests pass if available
* No build errors

---

## Implementation Workflow

For every implementation:

1. Check current project state.
2. Identify required files.
3. Create new directories/files through scripts.
4. Implement code.
5. Build verification.
6. Update documentation.
7. Mark task complete.

A task is incomplete until documentation is updated.

---

## Existing File Modification

When modifying existing files:

1. Confirm current file content.
2. Specify exact changes.
3. Apply replacement.
4. Build verify.

Avoid ambiguous modifications.

