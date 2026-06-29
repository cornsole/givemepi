#include "platform/CPUFeatures.hpp"
#include "platform/CPUInfo.hpp"

#include <iostream>


int main()
{
    using namespace pi::platform;


    CPUFeatures features =
        CPUFeatureDetector::detect();


    std::cout
        << "CPUID: "
        << features.cpuid_supported
        << '\n';


    std::cout
        << "SSE: "
        << features.sse
        << '\n';


    std::cout
        << "SSE2: "
        << features.sse2
        << '\n';


    std::cout
        << "AVX: "
        << features.hasAVX()
        << '\n';


    std::cout
        << "AVX2: "
        << features.hasAVX2()
        << '\n';


    std::cout
        << "AVX512: "
        << features.hasAVX512()
        << '\n';



    CPUInfo info =
        CPUInfo::detect();


    std::cout
        << "\nCPU Information\n"
        << "---------------\n"
        << info.summary();



    if (!features.cpuid_supported)
    {
        std::cerr
            << "CPUID not supported\n";

        return 1;
    }


    return 0;
}