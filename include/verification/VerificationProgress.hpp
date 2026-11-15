#pragma once

#include <cstdint>


namespace pi::verification
{

/// Receives completed verification work units on the verifying thread.
///
/// Verification components publish raw units (bytes, samples); FinalVerifier
/// scales them into its planned work units. Implementations must not throw,
/// format, or block. Time and memory complexity: O(1) expected.
class VerificationWorkObserver
{
public:
    virtual ~VerificationWorkObserver() = default;

    /// Adds completed units; called only with a non-zero count.
    virtual void workCompleted(std::uint64_t units) noexcept = 0;

protected:
    VerificationWorkObserver() = default;
    VerificationWorkObserver(const VerificationWorkObserver&) = default;
    VerificationWorkObserver& operator=(const VerificationWorkObserver&) = default;
};


/// Forwards raw units multiplied by a fixed weight to another observer.
///
/// A null target makes every call a no-op. Multiplication saturates at
/// UINT64_MAX. Time and memory complexity: O(1).
class ScaledVerificationWork final : public VerificationWorkObserver
{
public:
    ScaledVerificationWork(
        VerificationWorkObserver* target,
        std::uint64_t weight
    ) noexcept
        : target_(target),
          weight_(weight)
    {
    }

    void workCompleted(std::uint64_t units) noexcept override
    {
        if (target_ == nullptr || units == 0 || weight_ == 0)
        {
            return;
        }
        constexpr std::uint64_t maximum = ~std::uint64_t{0};
        target_->workCompleted(
            units > maximum / weight_ ? maximum : units * weight_
        );
    }

private:
    VerificationWorkObserver* target_;
    std::uint64_t weight_;
};

} // namespace pi::verification
