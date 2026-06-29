#include "platform/CPUFeatures.hpp"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <cpuid.h>

#endif


namespace pi::platform
{

bool CPUFeatures::hasAVX() const noexcept
{
    return avx && osxsave;
}


bool CPUFeatures::hasAVX2() const noexcept
{
    return hasAVX() && avx2;
}


bool CPUFeatures::hasAVX512() const noexcept
{
    return hasAVX() && avx512_f;
}


CPUFeatures CPUFeatureDetector::detect() noexcept
{
    CPUFeatures features{};

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;


    if (!__get_cpuid_max(0, nullptr))
    {
        return features;
    }


    features.cpuid_supported = true;


    // ------------------------------------------------------------
    // Basic features
    // ------------------------------------------------------------

    __get_cpuid(
        1,
        &eax,
        &ebx,
        &ecx,
        &edx);


    features.sse =
        (edx & bit_SSE) != 0;

    features.sse2 =
        (edx & bit_SSE2) != 0;

    features.sse3 =
        (ecx & bit_SSE3) != 0;

    features.ssse3 =
        (ecx & bit_SSSE3) != 0;

    features.sse4_1 =
        (ecx & bit_SSE4_1) != 0;

    features.sse4_2 =
        (ecx & bit_SSE4_2) != 0;


    features.avx =
        (ecx & bit_AVX) != 0;


    features.osxsave =
        (ecx & bit_OSXSAVE) != 0;


    if (features.osxsave)
    {
        features.xcr0 = readXCR0();

        // XMM + YMM state enabled
        if ((features.xcr0 & 0x6) != 0x6)
        {
            features.avx = false;
        }
    }


    // ------------------------------------------------------------
    // Extended features
    // ------------------------------------------------------------

    if (__get_cpuid_max(0, nullptr) >= 7)
    {
        __cpuid_count(
            7,
            0,
            eax,
            ebx,
            ecx,
            edx);


        features.avx2 =
            (ebx & bit_AVX2) != 0;


        features.avx512_f =
            (ebx & bit_AVX512F) != 0;


        features.avx512_cd =
            (ebx & bit_AVX512CD) != 0;


        features.avx512_bw =
            (ebx & bit_AVX512BW) != 0;


        features.avx512_dq =
            (ebx & bit_AVX512DQ) != 0;


        features.avx512_vl =
            (ebx & bit_AVX512VL) != 0;


        // AVX512 requires:
        // XMM + YMM + ZMM state
        if ((features.xcr0 & 0xE6) != 0xE6)
        {
            features.avx512_f = false;
            features.avx512_cd = false;
            features.avx512_bw = false;
            features.avx512_dq = false;
            features.avx512_vl = false;
        }
    }


#endif

    return features;
}


bool CPUFeatureDetector::checkOSXSAVE() noexcept
{
    return detect().osxsave;
}


std::uint64_t CPUFeatureDetector::readXCR0() noexcept
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

    unsigned int eax = 0;
    unsigned int edx = 0;

#if defined(_MSC_VER)

    return _xgetbv(0);

#else

    asm volatile(
        "xgetbv"
        : "=a"(eax), "=d"(edx)
        : "c"(0)
    );

#endif

    return (static_cast<std::uint64_t>(edx) << 32) | eax;

#else

    return 0;

#endif
}


} // namespace pi::platform