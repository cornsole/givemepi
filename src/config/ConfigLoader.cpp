#include "config/ConfigLoader.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

#include <toml++/toml.hpp>

namespace pi::config
{

namespace
{

void validate(Config& config)
{
    if (config.digits == 0)
    {
        throw std::runtime_error(
            "Config validation failed: digits must be greater than 0.");
    }

    if (config.threads > 1024)
    {
        throw std::runtime_error(
            "Config validation failed: threads value is too large.");
    }

    if (config.checkpoint_interval_seconds == 0 &&
        config.checkpoint_enabled)
    {
        throw std::runtime_error(
            "Config validation failed: checkpoint interval must be greater than 0.");
    }

    if (config.progress_interval_ms == 0)
    {
        throw std::runtime_error(
            "Config validation failed: progress interval must be greater than 0.");
    }

    if (config.output_file.empty())
    {
        throw std::runtime_error(
            "Config validation failed: output file is empty.");
    }

    if (config.compression.empty())
    {
        config.compression = "none";
    }
}

template <typename T>
void load_value(
    const toml::table& table,
    const char* key,
    T& target)
{
    if (auto value = table[key].value<T>())
    {
        target = *value;
    }
}

} // namespace


Config ConfigLoader::load(
    const std::filesystem::path& path)
{
    Config config{};

    if (!std::filesystem::exists(path))
    {
        validate(config);
        return config;
    }

    try
    {
        toml::table table =
            toml::parse_file(path.string());

        load_value(
            table,
            "digits",
            config.digits);

        load_value(
            table,
            "threads",
            config.threads);

        load_value(
            table,
            "checkpoint",
            config.checkpoint_enabled);

        load_value(
            table,
            "checkpoint_interval",
            config.checkpoint_interval_seconds);

        load_value(
            table,
            "huge_pages",
            config.huge_pages_enabled);

        load_value(
            table,
            "out_of_core",
            config.out_of_core_enabled);

        load_value(
            table,
            "compression",
            config.compression);

        load_value(
            table,
            "progress_interval",
            config.progress_interval_ms);

        load_value(
            table,
            "output",
            config.output_file);

        load_value(
            table,
            "resume",
            config.resume_enabled);
    }
    catch (const toml::parse_error& e)
    {
        throw std::runtime_error(
            "Config TOML parse error: " +
            std::string(e.description()));
    }

    validate(config);

    return config;
}

} // namespace pi::config