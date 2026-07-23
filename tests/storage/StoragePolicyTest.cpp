#include "storage/StoragePolicy.hpp"

#include <iostream>
#include <stdexcept>


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

} // namespace


int main()
{
    using pi::storage::CompressionAlgorithm;
    using pi::storage::StoragePolicy;

    const StoragePolicy defaults;
    defaults.validate();
    if (!defaults.enabled
        || defaults.directory != "storage"
        || defaults.memory_budget_bytes != 512ULL * 1024ULL * 1024ULL
        || defaults.target_chunk_size_bytes != 64ULL * 1024ULL * 1024ULL
        || defaults.compression != CompressionAlgorithm::none
        || defaults.max_concurrent_io != 1)
    {
        std::cerr << "Storage policy defaults mismatch\n";
        return 1;
    }

    StoragePolicy custom;
    custom.enabled = false;
    custom.directory = "/tmp/pi-storage";
    custom.memory_budget_bytes = 1024;
    custom.target_chunk_size_bytes = 512;
    custom.compression = CompressionAlgorithm::lz4;
    custom.max_concurrent_io = 4;
    custom.validate();

    if (pi::storage::parseCompressionAlgorithm("none")
            != CompressionAlgorithm::none
        || pi::storage::parseCompressionAlgorithm("lz4")
            != CompressionAlgorithm::lz4
        || pi::storage::toString(CompressionAlgorithm::lz4) != "lz4")
    {
        std::cerr << "Storage compression spelling mismatch\n";
        return 1;
    }

    if (!throws<std::invalid_argument>([] {
            StoragePolicy invalid;
            invalid.directory.clear();
            invalid.validate();
        })
        || !throws<std::invalid_argument>([] {
            StoragePolicy invalid;
            invalid.memory_budget_bytes = 0;
            invalid.validate();
        })
        || !throws<std::invalid_argument>([] {
            StoragePolicy invalid;
            invalid.target_chunk_size_bytes =
                invalid.memory_budget_bytes + 1;
            invalid.validate();
        })
        || !throws<std::invalid_argument>([] {
            StoragePolicy invalid;
            invalid.max_concurrent_io = 0;
            invalid.validate();
        })
        || !throws<std::invalid_argument>([] {
            static_cast<void>(
                pi::storage::parseCompressionAlgorithm("zstd")
            );
        }))
    {
        std::cerr << "Invalid storage policy was accepted\n";
        return 1;
    }

    std::cout << "StoragePolicy OK\n";
    return 0;
}
