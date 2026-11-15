#pragma once

#include <cstdint>
#include <string_view>


namespace pi::progress
{

/// Unit used to present a human-readable ETA.
enum class EtaUnit
{
    /// `sec` below one minute, `min` below one hour, `hour` otherwise.
    automatic,
    /// Always minutes.
    minutes,
    /// Always hours.
    hours
};


/// Presentation policy for human-readable ETA values.
///
/// Formatting only: ETA derivation and machine-readable nanosecond values
/// are unaffected. Values render in fixed-point notation with exactly
/// `precision` fractional digits and the classic locale.
struct EtaFormat
{
    /// Largest precision; nine fractional digits of a second reach the
    /// nanosecond resolution of the underlying duration.
    static constexpr std::uint32_t maxPrecision = 9;

    EtaUnit unit = EtaUnit::automatic;
    /// Fractional digits, 0 through `maxPrecision`.
    std::uint32_t precision = 2;

    [[nodiscard]]
    bool operator==(const EtaFormat&) const noexcept = default;
};


/// Returns the stable command-line spelling of an ETA unit:
/// `auto`, `min`, or `hour`.
/// Time and memory complexity: O(1).
[[nodiscard]]
constexpr std::string_view toString(EtaUnit unit) noexcept
{
    switch (unit)
    {
        case EtaUnit::automatic: return "auto";
        case EtaUnit::minutes: return "min";
        case EtaUnit::hours: return "hour";
    }
    return "unknown";
}

} // namespace pi::progress
