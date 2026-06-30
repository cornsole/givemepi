# Changelog

## [0.4.0] - PR-0008

### Added

- Added WorkStealingQueue scheduler component.
- Added worker local queue execution path.
- Added worker stealing support.
- Added WorkStealingTest.

### Changed

- Extended Worker execution model with local queue processing.
- Updated ThreadPool worker coordination for work stealing.
- Extended scheduler execution flow for distributed task processing.

### Fixed

- Fixed missing work stealing scheduler validation coverage.

## [0.4.0] - PR-0007

### Added

- Added ThreadPool implementation.
- Added ThreadPool worker lifecycle management.
- Added ThreadPool tests.

### Changed

- Scheduler now delegates worker management to ThreadPool.
- Scheduler no longer manages Worker instances directly.
- Scheduler execution flow now uses ThreadPool abstraction.

### Fixed

- Fixed scheduler ownership separation between Scheduler and Worker layers.

## [0.4.0] - PR-0006

### Added

- Added scheduler queue abstraction.
- Added queue interface layer.
- Added ReferenceQueue implementation.
- Added LockFreeQueue implementation.

### Changed

- Updated Scheduler to support interchangeable queue backends.
- Updated Worker to operate with scheduler queue implementations.
- Separated scheduler architecture validation from queue optimization.

### Fixed

- Fixed LockFreeQueue enqueue position handling.
- Fixed LockFreeQueue dequeue position handling.
- Fixed scheduler queue replacement test coverage.

## [0.4.0] - PR-0005

### Added

- Added Scheduler foundation.
- Added Task abstraction.
- Added Worker implementation.
- Added Scheduler implementation.
- Added Reference Queue implementation.
- Added scheduler subsystem tests.

### Changed

- Extended CMake build system with scheduler sources.
- Added scheduler-test target.
- Split Scheduler development into Reference Queue and Lock-Free Queue phases.

### Fixed

- Fixed scheduler build integration.
- Fixed queue implementation architecture for staged development.

## [0.3.0] - PR-0004

### Added

- Added Memory Layer foundation.
- Added Arena allocator.
- Added Memory Pool allocator.
- Added Alignment utility.
- Added Scratch Buffer.
- Added memory subsystem tests.

### Changed

- Extended CMake build system with memory sources.
- Added memory-test target.

### Fixed

- N/A

## [0.2.0] - PR-0003

### Added

- Added runtime CPU feature detection.
- Added CPUID detection.
- Added AVX, AVX2, AVX512 detection.
- Added CPU information reporting.
- Added platform detection tests.

### Changed

- Extended CMake build system with platform test target.

## [0.1.0] - PR-0002-1

### Added

- Reorganized project documentation under `docs/`.
- Created initial scheduler module.
- Added `include/scheduler/ThreadPool.hpp`.
- Added `src/scheduler/ThreadPool.cpp`.
- Established namespace `pi::scheduler`.
- Added initial non-copyable/non-movable `ThreadPool` class skeleton for future implementation.

- Added configuration system foundation.
- Added `include/config/Defaults.hpp`.
- Added `include/config/Config.hpp`.
- Added `src/config/Config.cpp`.
- Added `include/config/ConfigLoader.hpp`.
- Added `src/config/ConfigLoader.cpp`.
- Added `include/config/CommandLine.hpp`.
- Added `src/config/CommandLine.cpp`.
- Added default configuration values.
- Added command-line configuration override support.
- Added `config.toml.example`.

- Added toml++ integration.
- Added `scripts/install-tomlpp.sh`.
- Added third-party dependency setup workflow for toml++.
- Added TOML configuration parsing support.
- Added configuration validation.


### Changed

- Updated CMake build configuration.
- Added toml++ include path.
- Enabled header-only toml++ usage.
- Updated ConfigLoader from stub implementation to TOML-based implementation.


### Fixed

- Fixed ConfigLoader temporary runtime failure state.
- Fixed missing TOML include integration after dependency installation.
- Fixed configuration loading workflow.


---

## [Unreleased]

### Added

- Logger system.
- Platform detection.
- Memory subsystem.
- Lock-free scheduler.
- Work stealing scheduler.
- Big integer backend.
- Binary splitting engine.
- Chudnovsky calculation core.