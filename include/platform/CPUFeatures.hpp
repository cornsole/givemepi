#pragma once

#include <cstdint>

namespace pi::platform
{

struct CPUFeatures
{
    bool cpuid_supported = false;

    bool sse = false;
    bool sse2 = false;
    bool sse3 = false;
    bool ssse3 = false;
    bool sse4_1 = false;
    bool sse4_2 = false;

    bool avx = false;
    bool avx2 = false;

    bool avx512_f = false;
    bool avx512_cd = false;
    bool avx512_bw = false;
    bool avx512_dq = false;
    bool avx512_vl = false;

    bool osxsave = false;

    std::uint64_t xcr0 = 0;

    [[nodiscard]]
    bool hasAVX() const noexcept;

    [[nodiscard]]
    bool hasAVX2() const noexcept;

    [[nodiscard]]
    bool hasAVX512() const noexcept;
};


class CPUFeatureDetector
{
public:

    static CPUFeatures detect() noexcept;

private:

    static bool checkOSXSAVE() noexcept;

    static std::uint64_t readXCR0() noexcept;
};


} // namespace pi::platform