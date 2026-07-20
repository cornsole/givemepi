# CHECKLIST

## Foundation

[x] CMake

[ ] Logger

[x] Config

[x] CLI

[x] Project directory structure

[x] README Algorithm, Architecture Direction, and Current Status

[x] Initial scheduler module skeleton

-----------------

## Automation

[x] CTest

[x] GitHub Actions CI

[x] Automated Build

[x] Automated Test

[x] GCC Build Matrix

[x] Clang Build Matrix

[x] clang-format Check

[x] clang-tidy Static Analysis

[x] AddressSanitizer

[x] UBSan

[x] Artifact Upload

[ ] Benchmark Workflow

[ ] Release Workflow


-----------------


## Platform

[x] CPUID

[x] AVX Detect

[x] AVX2 Detect

[x] AVX512 Detect

[ ] Huge Pages

[ ] NUMA

-----------------

## Memory

[x] Arena

[x] Pool

[x] Alignment

[x] Scratch Buffer

-----------------

## Scheduler

[x] Task

[x] Worker

[x] Scheduler

[x] ThreadPool

[x] Reference Queue

[x] Lock-Free Queue

[x] Work Stealing

[x] Task Synchronization

[x] Task Handle / Future

[x] Task Join

[x] Shared Task State

[x] Task Execute Lifecycle

[x] Task Exception Capture

[x] TaskHandle Exception Inspection and Rethrow

[x] TaskHandle Validity

[x] TaskHandle Completion Wait

[x] Scheduler / ThreadPool Unified Submit Handle API

[x] Scheduler Synchronization Test Scenarios

[x] Scheduler Lifecycle State

[x] Concurrent Start / Stop Serialization

[x] Idempotent Start / Stop

[x] Scheduler Restart After Stop

[x] Submission Rejection While Stopping

[x] Drain Shutdown Semantics

[x] Accepted Outstanding Task Accounting

[x] Two-Phase Worker Stop and Join

[x] Global / Local Queue Role Contract

[x] Cross-Pool Worker Submission Routing

[x] Global Queue Capacity Rejection

[x] Lock-Free Queue Capacity Validation

[x] Lock-Free Queue Sequence / Contention Correctness

[x] Lock-Free Queue Slot Reuse Coverage

[x] Lock-Free Queue Concurrent Tests

[x] Lock-Free Queue Exactly-Once MPMC Coverage

[x] Concurrent Submit / Stop Acceptance Cutoff Test

[x] Concurrent Accepted / Rejected Execution Accounting

[x] Multi-Caller Repeated Start / Stop Test

[x] Dedicated Scheduler Concurrency Test Target

[x] Work-Stealing Behavior Tests

[x] Worker-Local Child Concentration Test

[x] Multiple Thief Thread Observation

[x] Root / Thief Thread Identity Validation

[x] Global Fallback Exclusion Test Queue

[x] Work-Stealing Load / Behavior Test Separation

[x] Scheduler Correctness Hardening (PR-0017)

-----------------

## Big Integer

[x] GMP

[x] In-Place Sign Negation

[ ] FFT

-----------------

## Binary Splitting

[x] Node

[x] Merge

[x] Recursive Split

[x] Parallel API Refactoring

[x] Chudnovsky Leaf Integration

[x] Input Range Validation

[x] Leaf and Small-Range P/Q/T Correctness

[x] Small Known-Digits Integration Test

[x] Binary Splitting Boundary and Regression Coverage

[x] Parallel Cutoff Policy

[x] Parallel Execution

[x] Parallel Merge

[x] Staged Parallel DAG

[x] Parallel Submission Fallback

[x] Worker-Context Deadlock Avoidance

[x] Observable Parallel Binary Splitting Test

-----------------

## Chudnovsky

[ ] Formula

[ ] Precision Control

[x] Precision and Guard-Digit Policy

[x] Overflow-Safe Digit-to-Term Conversion

[x] Estimated Binary Precision

-----------------

## Storage

[ ] Compression

[ ] Chunk Index

[ ] Reload

-----------------

## Checkpoint

[ ] P/Q/T Block Format

[ ] Block Magic and Format Version

[ ] Range and Tree-Level Metadata

[ ] P/Q/T Payload Length Metadata

[ ] Checksum Type and Value Metadata

[ ] Atomic Temporary File Commit

[ ] Manifest

[ ] Save

[ ] Structural Block Validation

[ ] Payload Checksum Validation

[ ] Manifest Consistency Validation

[ ] Resume from Verified Blocks

[ ] Corrupt Block Quarantine

[ ] Corrupt Block Recalculation

[ ] CRC

-----------------

## Progress

[ ] Progress Phase

[ ] Thread-Safe Progress Tracker

[ ] Immutable Progress Snapshot

[ ] Total and Completed Terms

[ ] Total and Completed Blocks

[ ] Current Merge Level

[ ] Active and Queued Tasks

[ ] ETA

[ ] Speed

[ ] Memory

[ ] Chunk

[ ] Checkpoint Bytes

[ ] Last Verified Checkpoint

[ ] CLI Text Reporter

[ ] CLI JSON Reporter

[ ] TTY and Non-TTY Output Modes

[ ] Progress Enable / Disable Option

[ ] Progress Interval Option

[ ] Progress Format Option

[ ] Post-CLI Configuration Validation

[ ] Resume Progress Reconstruction

[ ] Future Reporter Extension Point

-----------------

## Verification

[ ] Independent Modular P/Q/T Verification

[ ] Hash

[ ] BBP

[ ] Known Digits

-----------------

## SIMD

[ ] AVX2

[ ] AVX512

-----------------

## ASM

[ ] CPUID

[ ] memcpy

[ ] Prefetch

[ ] rdtsc

-----------------

## Benchmark

[ ] Single Thread

[ ] Multi Thread

[ ] Memory

[ ] Storage
