#pragma once

#include <filesystem>

#include "config/Config.hpp"

namespace pi::config
{

class ConfigLoader
{
public:
    ConfigLoader() = delete;

    /// Load configuration from a TOML file.
    ///
    /// Behavior:
    /// - If the file does not exist, returns a Config initialized with defaults.
    /// - If the file exists but is invalid, throws an exception.
    /// - Missing keys fall back to default values.
    ///
    /// @param path Path to the configuration file.
    /// @return Loaded configuration.
    [[nodiscard]]
    static Config load(const std::filesystem::path& path);
};

} // namespace pi::config