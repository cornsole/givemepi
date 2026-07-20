#pragma once

#include "checkpoint/CheckpointValidation.hpp"

#include <filesystem>
#include <optional>


namespace pi::checkpoint
{

struct QuarantineOutcome
{
    ValidationResult operation;
    std::optional<std::filesystem::path> quarantinedPath;
    std::optional<std::filesystem::path> diagnosticPath;
};


class CheckpointQuarantine
{
public:
    explicit CheckpointQuarantine(std::filesystem::path directory);

    [[nodiscard]]
    QuarantineOutcome quarantine(
        const std::filesystem::path& source,
        const ValidationResult& rejection
    ) const;

private:
    std::filesystem::path directory_;
};

} // namespace pi::checkpoint
