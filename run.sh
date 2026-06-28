#!/usr/bin/env bash
set -e

mkdir -p include/config
mkdir -p src/config

touch include/config/Config.hpp
touch include/config/Defaults.hpp
touch include/config/ConfigLoader.hpp
touch include/config/CommandLine.hpp

touch src/config/Config.cpp
touch src/config/ConfigLoader.cpp
touch src/config/CommandLine.cpp

touch config.toml.example

mkdir -p third_party/tomlplusplus