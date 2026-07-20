#include "config/ConfigLoader.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

#include <toml++/toml.hpp>

namespace pi::config
{

namespace
{

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
        validateConfig(config);
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
            "progress",
            config.progress_enabled);

        load_value(
            table,
            "progress_interval",
            config.progress_interval_ms);

        std::string progressFormat = std::string(toString(
            config.progress_format
        ));
        load_value(table, "progress_format", progressFormat);
        config.progress_format = parseProgressFormat(progressFormat);

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

    validateConfig(config);

    return config;
}

} // namespace pi::config
