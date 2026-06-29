#include "platform/CPUInfo.hpp"

#include <sstream>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

#include <cpuid.h>

#endif


namespace pi::platform
{

CPUInfo CPUInfo::detect() noexcept
{
    CPUInfo info{};

    info.features =
        CPUFeatureDetector::detect();


    info.logical_processors =
        static_cast<std::uint32_t>(
            std::thread::hardware_concurrency());


#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;


    // Vendor string
    if (__get_cpuid_max(0, nullptr))
    {
        __get_cpuid(
            0,
            &eax,
            &ebx,
            &ecx,
            &edx);


        char vendor[13]{};

        *reinterpret_cast<unsigned int*>(&vendor[0]) = ebx;
        *reinterpret_cast<unsigned int*>(&vendor[4]) = edx;
        *reinterpret_cast<unsigned int*>(&vendor[8]) = ecx;


        info.vendor = vendor;
    }


    // Brand string
    if (__get_cpuid_max(0x80000000, nullptr) >= 0x80000004)
    {
        char brand[49]{};

        unsigned int* data =
            reinterpret_cast<unsigned int*>(brand);


        for (unsigned int i = 0; i < 3; ++i)
        {
            __cpuid(
                0x80000002 + i,
                data[i * 4],
                data[i * 4 + 1],
                data[i * 4 + 2],
                data[i * 4 + 3]);
        }


        info.brand = brand;
    }


#endif


    return info;
}


std::string CPUInfo::summary() const
{
    std::ostringstream out;


    out
        << "CPU Vendor: "
        << vendor
        << '\n';


    out
        << "CPU Brand: "
        << brand
        << '\n';


    out
        << "Logical Processors: "
        << logical_processors
        << '\n';


    out
        << "Features:\n";


    out
        << "  SSE: "
        << features.sse
        << '\n';


    out
        << "  AVX: "
        << features.hasAVX()
        << '\n';


    out
        << "  AVX2: "
        << features.hasAVX2()
        << '\n';


    out
        << "  AVX512: "
        << features.hasAVX512()
        << '\n';


    return out.str();
}


} // namespace pi::platform