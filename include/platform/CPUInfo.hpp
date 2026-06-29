#pragma once

#include <string>
#include <cstdint>

#include "platform/CPUFeatures.hpp"

namespace pi::platform
{

struct CPUInfo
{
    std::string vendor;
    std::string brand;

    std::uint32_t logical_processors = 0;
    std::uint32_t physical_cores = 0;

    CPUFeatures features{};


    [[nodiscard]]
    static CPUInfo detect() noexcept;


    [[nodiscard]]
    std::string summary() const;
};


} // namespace pi::platform