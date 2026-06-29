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
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------

add_executable(pi-engine
    ${PI_ENGINE_SOURCES}
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

# find_package(GMP REQUIRED)
# target_link_libraries(pi-engine PRIVATE GMP::gmp)

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