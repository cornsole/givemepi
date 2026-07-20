#include "config/Config.hpp"

#include <stdexcept>


namespace pi::config
{

std::string_view toString(ProgressFormat format) noexcept
{
    switch (format)
    {
        case ProgressFormat::text: return "text";
        case ProgressFormat::json: return "json";
    }
    return "unknown";
}


ProgressFormat parseProgressFormat(std::string_view value)
{
    if (value == "text")
    {
        return ProgressFormat::text;
    }
    if (value == "json")
    {
        return ProgressFormat::json;
    }
    throw std::invalid_argument(
        "Unsupported progress format: " + std::string(value)
    );
}


void validateConfig(Config& config)
{
    if (config.digits == 0)
    {
        throw std::runtime_error(
            "Config validation failed: digits must be greater than 0."
        );
    }
    if (config.threads > 1024)
    {
        throw std::runtime_error(
            "Config validation failed: threads value is too large."
        );
    }
    if (config.checkpoint_enabled
        && config.checkpoint_interval_seconds == 0)
    {
        throw std::runtime_error(
            "Config validation failed: checkpoint interval must be greater than 0."
        );
    }
    if (config.progress_interval_ms == 0)
    {
        throw std::runtime_error(
            "Config validation failed: progress interval must be greater than 0."
        );
    }
    if (config.output_file.empty())
    {
        throw std::runtime_error(
            "Config validation failed: output file is empty."
        );
    }
    if (config.compression.empty())
    {
        config.compression = "none";
    }
}

} // namespace pi::config
