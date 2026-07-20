#pragma once

#include "checkpoint/CheckpointBlockCodec.hpp"
#include "checkpoint/CheckpointValidation.hpp"

#include <array>
#include <cstdint>


namespace pi::checkpoint
{

class ModularPQTVerifier
{
public:
    static constexpr std::array<std::uint32_t, 3> verificationPrimes{
        998244353U,
        1000000007U,
        1000000009U
    };

    [[nodiscard]]
    static ValidationResult validate(const CheckpointBlock& block);
};

} // namespace pi::checkpoint
