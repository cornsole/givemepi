cmake_minimum_required(VERSION 3.20)

project(
    PiEngine
    VERSION 0.1.0
    DESCRIPTION "High Performance Pi Calculator"
    LANGUAGES CXX
)

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

add_executable(scheduler-test
    tests/scheduler/SchedulerTest.cpp
    
    src/scheduler/Task.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
)

add_executable(threadpool-test
    tests/scheduler/ThreadPoolTest.cpp

    src/scheduler/Task.cpp
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
)


add_executable(gmp-test
    tests/bigint/GMPIntegerTest.cpp

    src/bigint/GMPInteger.cpp
)


add_executable(binary-test
    tests/binary/BinarySplittingTest.cpp

    src/binary/BinaryNode.cpp
    src/binary/BinarySplitter.cpp

    src/bigint/GMPInteger.cpp
)


target_include_directories(binary-test
    PRIVATE
        include
)


target_link_libraries(binary-test
    PRIVATE
        gmp
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
    src/scheduler/ThreadPool.cpp
    src/scheduler/LockFreeQueue.cpp
    src/scheduler/WorkStealingQueue.cpp
    src/scheduler/Worker.cpp
    src/scheduler/Scheduler.cpp
    src/scheduler/ReferenceQueue.cpp
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