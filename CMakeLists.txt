cmake_minimum_required(VERSION 3.20)

project(
    PiEngine
    VERSION 0.1.0
    DESCRIPTION "High Performance Pi Calculator"
    LANGUAGES CXX
)

enable_testing()
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# C++
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Compiler Options
# ---------------------------------------------------------------------------

if(MSVC)

    add_compile_options(/W4)

else()

    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        add_compile_options(
            -march=native
        )
    endif()

endif()

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

option(ENABLE_BENCHMARKS "Build standalone benchmark executables" OFF)


if(ENABLE_ASAN)

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=address
    )

endif()


if(ENABLE_BENCHMARKS)

    add_executable(chudnovsky-benchmark
        benchmarks/chudnovsky/ChudnovskyBenchmark.cpp

        src/chudnovsky/PrecisionPolicy.cpp
        src/chudnovsky/ChudnovskyCalculator.cpp

        src/binary/BinaryNode.cpp
        src/binary/BinarySplitter.cpp
        src/bigint/GMPInteger.cpp

        src/scheduler/Task.cpp
        src/scheduler/TaskHandle.cpp
        src/scheduler/ThreadPool.cpp
        src/scheduler/LockFreeQueue.cpp
        src/scheduler/WorkStealingQueue.cpp
        src/scheduler/Worker.cpp
        src/scheduler/Scheduler.cpp
        src/scheduler/ReferenceQueue.cpp
    )

    target_include_directories(chudnovsky-benchmark
        PRIVATE
            include
    )

    target_link_libraries(chudnovsky-benchmark
        PRIVATE
            gmp
            Threads::Threads
    )

endif()


if(ENABLE_UBSAN)

    add_compile_options(
        -fsanitize=undefined
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=undefined
    )

endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

set(PI_ENGINE_SOURCES
    src/main.cpp

    src/config/Config.cpp
    src/config/ConfigLoader.cpp
    src/config/CommandLine.cpp

    src/scheduler/ThreadPool.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp

    src/platform/CPUFeatures.cpp
    src/platform/CPUInfo.cpp

    src/memory/Arena.cpp
    src/memory/MemoryPool.cpp
    src/memory/Alignment.cpp
    src/memory/ScratchBuffer.cpp

    src/bigint/GMPInteger.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp

    src/chudnovsky/PrecisionPolicy.cpp
    src/chudnovsky/ChudnovskyCalculator.cpp

    src/checkpoint/CheckpointTypes.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/ManifestEntryValidator.cpp
    src/checkpoint/CheckpointManifestValidator.cpp
    src/checkpoint/ModularPQTVerifier.cpp
    src/checkpoint/CheckpointQuarantine.cpp
    src/checkpoint/CheckpointResumePlanner.cpp
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------

add_executable(pi-engine
    ${PI_ENGINE_SOURCES}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

add_executable(cpu-features-test
    tests/platform/CPUFeaturesTest.cpp

    src/platform/CPUFeatures.cpp
    src/platform/CPUInfo.cpp
)

add_executable(precision-policy-test
    tests/chudnovsky/PrecisionPolicyTest.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_executable(chudnovsky-calculator-test
    tests/chudnovsky/ChudnovskyCalculatorTest.cpp

    src/chudnovsky/PrecisionPolicy.cpp
    src/chudnovsky/ChudnovskyCalculator.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME ChudnovskyCalculatorTest
    COMMAND chudnovsky-calculator-test
)

target_include_directories(chudnovsky-calculator-test
    PRIVATE
        include
)

target_link_libraries(chudnovsky-calculator-test
    PRIVATE
        gmp
        Threads::Threads
)

add_test(
    NAME PrecisionPolicyTest
    COMMAND precision-policy-test
)

target_include_directories(precision-policy-test
    PRIVATE
        include
)

add_executable(checkpoint-types-test
    tests/checkpoint/CheckpointTypesTest.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
)

add_test(
    NAME CheckpointTypesTest
    COMMAND checkpoint-types-test
)

target_include_directories(checkpoint-types-test
    PRIVATE
        include
)

add_executable(gmp-serialization-test
    tests/checkpoint/GMPSerializationTest.cpp
    src/checkpoint/GMPSerialization.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME GMPSerializationTest
    COMMAND gmp-serialization-test
)

target_include_directories(gmp-serialization-test
    PRIVATE
        include
)

target_link_libraries(gmp-serialization-test
    PRIVATE
        gmp
)

add_executable(checkpoint-block-codec-test
    tests/checkpoint/CheckpointBlockCodecTest.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointBlockCodecTest
    COMMAND checkpoint-block-codec-test
)

target_include_directories(checkpoint-block-codec-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-block-codec-test
    PRIVATE
        gmp
)

add_executable(crc32c-test
    tests/checkpoint/CRC32CTest.cpp
    src/checkpoint/CRC32C.cpp
)

add_test(
    NAME CRC32CTest
    COMMAND crc32c-test
)

target_include_directories(crc32c-test
    PRIVATE
        include
)

add_executable(checkpoint-store-test
    tests/checkpoint/CheckpointStoreTest.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointStoreTest
    COMMAND checkpoint-store-test
)

target_include_directories(checkpoint-store-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-store-test
    PRIVATE
        gmp
)

add_executable(checkpoint-manifest-test
    tests/checkpoint/CheckpointManifestTest.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointManifestTest
    COMMAND checkpoint-manifest-test
)

target_include_directories(checkpoint-manifest-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-manifest-test
    PRIVATE
        gmp
)

add_executable(checkpoint-validation-test
    tests/checkpoint/CheckpointValidationTest.cpp
    src/checkpoint/CheckpointValidation.cpp
)

add_test(
    NAME CheckpointValidationTest
    COMMAND checkpoint-validation-test
)

target_include_directories(checkpoint-validation-test
    PRIVATE
        include
)

add_executable(checkpoint-file-inspector-test
    tests/checkpoint/CheckpointFileInspectorTest.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointFileInspectorTest
    COMMAND checkpoint-file-inspector-test
)

target_include_directories(checkpoint-file-inspector-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-file-inspector-test
    PRIVATE
        gmp
)

add_executable(checkpoint-block-validator-test
    tests/checkpoint/CheckpointBlockValidatorTest.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointBlockValidatorTest
    COMMAND checkpoint-block-validator-test
)

target_include_directories(checkpoint-block-validator-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-block-validator-test
    PRIVATE
        gmp
)

add_executable(manifest-entry-validator-test
    tests/checkpoint/ManifestEntryValidatorTest.cpp
    src/checkpoint/ManifestEntryValidator.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME ManifestEntryValidatorTest
    COMMAND manifest-entry-validator-test
)

target_include_directories(manifest-entry-validator-test
    PRIVATE
        include
)

target_link_libraries(manifest-entry-validator-test
    PRIVATE
        gmp
)

add_executable(checkpoint-manifest-validator-test
    tests/checkpoint/CheckpointManifestValidatorTest.cpp
    src/checkpoint/CheckpointManifestValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/checkpoint/CRC32C.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
)

add_test(
    NAME CheckpointManifestValidatorTest
    COMMAND checkpoint-manifest-validator-test
)

target_include_directories(checkpoint-manifest-validator-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-manifest-validator-test
    PRIVATE
        gmp
)

add_executable(modular-pqt-verifier-test
    tests/checkpoint/ModularPQTVerifierTest.cpp
    src/checkpoint/ModularPQTVerifier.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME ModularPQTVerifierTest
    COMMAND modular-pqt-verifier-test
)

target_include_directories(modular-pqt-verifier-test
    PRIVATE
        include
)

target_link_libraries(modular-pqt-verifier-test
    PRIVATE
        gmp
        Threads::Threads
)

add_executable(checkpoint-quarantine-test
    tests/checkpoint/CheckpointQuarantineTest.cpp
    src/checkpoint/CheckpointQuarantine.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/bigint/GMPInteger.cpp
)

add_test(
    NAME CheckpointQuarantineTest
    COMMAND checkpoint-quarantine-test
)

target_include_directories(checkpoint-quarantine-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-quarantine-test
    PRIVATE
        gmp
)

add_executable(checkpoint-resume-planner-test
    tests/checkpoint/CheckpointResumePlannerTest.cpp
    src/checkpoint/CheckpointResumePlanner.cpp
    src/checkpoint/CheckpointQuarantine.cpp
    src/checkpoint/ModularPQTVerifier.cpp
    src/checkpoint/CheckpointManifestValidator.cpp
    src/checkpoint/ManifestEntryValidator.cpp
    src/checkpoint/CheckpointFileInspector.cpp
    src/checkpoint/CheckpointBlockValidator.cpp
    src/checkpoint/CheckpointValidation.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/CheckpointManifest.cpp
    src/checkpoint/AtomicFileCommit.cpp
    src/checkpoint/CheckpointBlockCodec.cpp
    src/checkpoint/CRC32C.cpp
    src/checkpoint/GMPSerialization.cpp
    src/checkpoint/CheckpointTypes.cpp
    src/chudnovsky/PrecisionPolicy.cpp
    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp
    src/bigint/GMPInteger.cpp
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME CheckpointResumePlannerTest
    COMMAND checkpoint-resume-planner-test
)

target_include_directories(checkpoint-resume-planner-test
    PRIVATE
        include
)

target_link_libraries(checkpoint-resume-planner-test
    PRIVATE
        gmp
        Threads::Threads
)

add_test(
    NAME CPUFeaturesTest
    COMMAND cpu-features-test
)

target_include_directories(cpu-features-test
    PRIVATE
        include
)

target_link_libraries(cpu-features-test
    PRIVATE
        Threads::Threads
)

# ---------------------------------------------------------------------------
# Memory tests
# ---------------------------------------------------------------------------

add_executable(memory-test
    tests/memory/MemoryTest.cpp

    src/memory/Arena.cpp
    src/memory/MemoryPool.cpp
    src/memory/Alignment.cpp
    src/memory/ScratchBuffer.cpp
)

add_test(
    NAME MemoryTest
    COMMAND memory-test
)


target_include_directories(memory-test
    PRIVATE
        include
)


target_link_libraries(memory-test
    PRIVATE
        Threads::Threads
)

# ---------------------------------------------------------------------------
# Include Directories
# ---------------------------------------------------------------------------

target_include_directories(pi-engine
    PRIVATE
        include
        third_party/tomlplusplus/include
)

target_compile_definitions(pi-engine
    PRIVATE
        TOML_HEADER_ONLY=1
)

# ---------------------------------------------------------------------------
# toml++
# ---------------------------------------------------------------------------
#
# Installed by:
#
# scripts/install-tomlpp.sh
#
# Repository:
#
# third_party/tomlplusplus
#
# Header-only library.
#
# Included via:
#
# third_party/tomlplusplus/include
#
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# GMP
#
# Will be integrated in a later PR.
# ---------------------------------------------------------------------------

target_link_libraries(pi-engine
    PRIVATE
        gmp
)

# ---------------------------------------------------------------------------
# Threads
#
# Will be required once ThreadPool is implemented.
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)

target_link_libraries(pi-engine
    PRIVATE
        Threads::Threads
)

add_executable(task-test
    tests/scheduler/TaskTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
)

add_test(
    NAME TaskTest
    COMMAND task-test
)

target_include_directories(task-test
    PRIVATE
        include
)

target_link_libraries(task-test
    PRIVATE
        Threads::Threads
)

add_executable(lockfreequeue-test
    tests/scheduler/LockFreeQueueTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/LockFreeQueue.cpp
)

add_test(
    NAME LockFreeQueueTest
    COMMAND lockfreequeue-test
)

target_include_directories(lockfreequeue-test
    PRIVATE
        include
)

target_link_libraries(lockfreequeue-test
    PRIVATE
        Threads::Threads
)

add_executable(scheduler-concurrency-test
    tests/scheduler/SchedulerConcurrencyTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
)

add_test(
    NAME SchedulerConcurrencyTest
    COMMAND scheduler-concurrency-test
)

target_include_directories(scheduler-concurrency-test
    PRIVATE
        include
)

target_link_libraries(scheduler-concurrency-test
    PRIVATE
        Threads::Threads
)

add_executable(scheduler-test
    tests/scheduler/SchedulerTest.cpp
    
    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME SchedulerTest
    COMMAND scheduler-test
)

add_executable(threadpool-test
    tests/scheduler/ThreadPoolTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
)

add_test(
    NAME ThreadPoolTest
    COMMAND threadpool-test
)


add_executable(gmp-test
    tests/bigint/GMPIntegerTest.cpp

    src/bigint/GMPInteger.cpp
)

add_test(
    NAME GMPIntegerTest
    COMMAND gmp-test
)

add_executable(binary-test
    tests/binary/BinarySplittingTest.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp

    src/bigint/GMPInteger.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME BinarySplittingTest
    COMMAND binary-test
)

target_include_directories(binary-test
    PRIVATE
        include
)


target_link_libraries(binary-test
    PRIVATE
        gmp
        Threads::Threads
)


target_include_directories(gmp-test
    PRIVATE
        include
)


target_link_libraries(gmp-test
    PRIVATE
        gmp
)


add_executable(workstealing-test
    tests/scheduler/WorkStealingTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_test(
    NAME WorkStealingTest
    COMMAND workstealing-test
)

add_executable(workstealing-behavior-test
    tests/scheduler/WorkStealingBehaviorTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/TaskHandle.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
)

add_test(
    NAME WorkStealingBehaviorTest
    COMMAND workstealing-behavior-test
)

target_include_directories(workstealing-behavior-test
    PRIVATE
        include
)

target_link_libraries(workstealing-behavior-test
    PRIVATE
        Threads::Threads
)

target_include_directories(workstealing-test
    PRIVATE
        include
)


target_link_libraries(workstealing-test
    PRIVATE
        Threads::Threads
)


target_include_directories(threadpool-test
    PRIVATE
        include
)


target_link_libraries(threadpool-test
    PRIVATE
        Threads::Threads
)

target_include_directories(scheduler-test
    PRIVATE
        include
)

target_link_libraries(scheduler-test
    PRIVATE
        Threads::Threads
)
