#include "config/CommandLine.hpp"
#include "config/ConfigLoader.hpp"
#include "storage/StoragePolicy.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>


namespace
{

template<typename Exception, typename Function>
bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }
    return false;
}


void apply(pi::config::Config& config, std::initializer_list<const char*> values)
{
    std::vector<char*> arguments;
    arguments.reserve(values.size());
    for (const char* value : values)
    {
        arguments.push_back(const_cast<char*>(value));
    }
    pi::config::CommandLine::applyOverrides(
        config,
        static_cast<int>(arguments.size()),
        arguments.data()
    );
}

} // namespace


int main()
{
    using pi::config::Config;
    using pi::config::ConfigLoader;
    using pi::config::ProgressFormat;

    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / ("givemepi-progress-config-" + std::to_string(::getpid()) + ".toml");
    {
        std::ofstream file(path);
        file << "progress = false\n"
             << "progress_interval = 750\n"
             << "progress_format = \"json\"\n"
             << "storage_directory = \"runtime-storage\"\n"
             << "storage_memory_budget_bytes = 1048576\n"
             << "storage_target_chunk_size_bytes = 262144\n"
             << "storage_max_concurrent_io = 3\n"
             << "verification = false\n"
             << "bbp_samples = 5\n"
             << "verification_manifest = \"result.verify\"\n";
    }

    Config config = ConfigLoader::load(path);
    std::filesystem::remove(path);
    assert(!config.progress_enabled);
    assert(config.progress_interval_ms == 750);
    assert(config.progress_format == ProgressFormat::json);
    assert(config.storage_directory == "runtime-storage");
    assert(config.storage_memory_budget_bytes == 1048576);
    assert(config.storage_target_chunk_size_bytes == 262144);
    assert(config.storage_max_concurrent_io == 3);
    const auto policy = pi::config::makeStoragePolicy(config);
    assert(policy.isEnabled());
    assert(policy.getDirectory() == "runtime-storage");
    assert(policy.getMemoryBudget() == 1048576);
    assert(policy.getTargetChunkSize() == 262144);
    assert(policy.getMaxConcurrentIo() == 3);
    assert(!config.verification_enabled);
    assert(config.bbp_sample_count == 5);
    assert(config.verification_manifest_file == "result.verify");

    apply(config, {
        "pi-engine",
        "--progress",
        "--progress-interval", "125",
        "--progress-format", "text",
        "--storage-directory", "cli-storage",
        "--storage-memory-budget-bytes", "2097152",
        "--storage-chunk-size-bytes", "524288",
        "--storage-max-concurrent-io", "4",
        "--verify",
        "--bbp-samples", "12",
        "--verification-manifest", "override.verify"
    });
    assert(config.progress_enabled);
    assert(config.progress_interval_ms == 125);
    assert(config.progress_format == ProgressFormat::text);
    assert(config.storage_directory == "cli-storage");
    assert(config.storage_memory_budget_bytes == 2097152);
    assert(config.storage_target_chunk_size_bytes == 524288);
    assert(config.storage_max_concurrent_io == 4);
    assert(config.verification_enabled);
    assert(config.bbp_sample_count == 12);
    assert(config.verification_manifest_file == "override.verify");

    Config disabled;
    apply(disabled, {"pi-engine", "--no-progress"});
    assert(!disabled.progress_enabled);

    assert(throws<std::runtime_error>([]() {
        Config invalid;
        apply(invalid, {"pi-engine", "--progress-interval", "0"});
    }));
    assert(throws<std::invalid_argument>([]() {
        Config invalid;
        apply(invalid, {"pi-engine", "--progress-format", "xml"});
    }));
    assert(throws<std::runtime_error>([]() {
        Config invalid;
        apply(invalid, {"pi-engine", "--storage-chunk-size-bytes", "0"});
    }));
    assert(throws<std::runtime_error>([]() {
        Config invalid;
        apply(invalid, {
            "pi-engine", "--storage-memory-budget-bytes", "1",
            "--storage-chunk-size-bytes", "2"
        });
    }));
    assert(throws<std::runtime_error>([]() {
        Config invalid;
        apply(invalid, {"pi-engine", "--bbp-samples", "0"});
    }));
    assert(throws<std::runtime_error>([]() {
        Config invalid;
        apply(invalid, {"pi-engine", "--bbp-samples", "33"});
    }));
    assert(throws<std::invalid_argument>([]() {
        Config invalid;
        apply(invalid, {"pi-engine", "--progress-interval"});
    }));
    assert(throws<std::invalid_argument>([]() {
        Config invalid;
        apply(invalid, {
            "pi-engine", "--progress-interval", "4294967296"
        });
    }));

    const std::filesystem::path invalidPath =
        std::filesystem::temp_directory_path()
        / ("givemepi-invalid-progress-" + std::to_string(::getpid()) + ".toml");
    {
        std::ofstream file(invalidPath);
        file << "progress_format = \"yaml\"\n";
    }
    assert(throws<std::invalid_argument>([&invalidPath]() {
        static_cast<void>(ConfigLoader::load(invalidPath));
    }));
    std::filesystem::remove(invalidPath);

    return 0;
}
