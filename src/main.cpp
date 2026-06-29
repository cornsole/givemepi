#include <iostream>

#include "config/CommandLine.hpp"
#include "config/ConfigLoader.hpp"

int main(int argc, char* argv[])
{
    pi::config::Config config =
        pi::config::ConfigLoader::load("config.toml");

    pi::config::CommandLine::applyOverrides(
        config,
        argc,
        argv);

    std::cout << "========== Pi Engine ==========\n";
    std::cout << "Digits               : " << config.digits << '\n';
    std::cout << "Threads              : " << config.threads << '\n';
    std::cout << "Checkpoint           : "
              << (config.checkpoint_enabled ? "Enabled" : "Disabled") << '\n';
    std::cout << "Checkpoint Interval  : "
              << config.checkpoint_interval_seconds << " sec\n";
    std::cout << "Huge Pages           : "
              << (config.huge_pages_enabled ? "Enabled" : "Disabled") << '\n';
    std::cout << "Out-of-Core          : "
              << (config.out_of_core_enabled ? "Enabled" : "Disabled") << '\n';
    std::cout << "Compression          : " << config.compression << '\n';
    std::cout << "Progress Interval    : "
              << config.progress_interval_ms << " ms\n";
    std::cout << "Output File          : " << config.output_file << '\n';
    std::cout << "Resume               : "
              << (config.resume_enabled ? "Enabled" : "Disabled") << '\n';

    return 0;
}