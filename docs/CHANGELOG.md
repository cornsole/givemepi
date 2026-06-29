# Changelog

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