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


if(ENABLE_ASAN)

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
    )

    add_link_options(
        -fsanitize=address
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

add_test(
    NAME PrecisionPolicyTest
    COMMAND precision-policy-test
)

target_include_directories(precision-policy-test
    PRIVATE
        include
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
