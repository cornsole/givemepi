# CHECKLIST

## Foundation

[x] CMake

[ ] Logger

[x] Config

[x] CLI

[x] Project directory structure

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

[ ] Drain Shutdown Semantics

[ ] Global / Local Queue Role Contract

[ ] Lock-Free Queue Concurrent Tests

[ ] Work-Stealing Behavior Tests

-----------------

## Big Integer

[x] GMP

[ ] FFT

-----------------

## Binary Splitting

[x] Node

[x] Merge

[x] Recursive Split

[x] Parallel API Refactoring

[ ] Chudnovsky Leaf Integration

[ ] Input Range Validation

[ ] Parallel Cutoff Policy

[ ] Parallel Execution

[ ] Parallel Merge

-----------------

## Chudnovsky

[ ] Formula

[ ] Precision Control

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
