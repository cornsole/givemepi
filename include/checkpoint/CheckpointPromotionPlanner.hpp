#pragma once

#include "checkpoint/CheckpointBlockCodec.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace pi::checkpoint
{

/// One fully validated source checkpoint imported by a promotion plan.
struct PromotedCheckpoint
{
    std::filesystem::path path;
    std::uint64_t fileSize;
    CheckpointBlock block;
};

/// Read-only, all-or-nothing lower-precision checkpoint promotion result.
struct CheckpointPromotionPlan
{
    ComputationIdentity sourceIdentity;
    std::vector<PromotedCheckpoint> promotedBlocks;
    std::uint64_t promotedTerms;
};

/// Validates an explicit lower-precision manifest for use as a target frontier.
///
/// The source manifest and blocks are read only. Every source range must be
/// present, valid, and mathematically verified; otherwise planning throws and
/// imports no blocks.
class CheckpointPromotionPlanner final
{
public:
    explicit CheckpointPromotionPlanner(std::filesystem::path manifestPath);

    /// Creates a complete promotion plan for a strictly larger compatible
    /// target. Time and memory complexity are linear in source block bytes.
    [[nodiscard]]
    CheckpointPromotionPlan create(
        const ComputationIdentity& targetIdentity
    ) const;

private:
    std::filesystem::path manifestPath_;
};

} // namespace pi::checkpoint
