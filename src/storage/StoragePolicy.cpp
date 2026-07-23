#include "storage/StoragePolicy.hpp"
#include <cmath>

#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace pi::storage
{

// Compression algorithm conversion
std::string_view toString(CompressionAlgorithm compression) noexcept
{
    switch (compression)
    {
        case CompressionAlgorithm::none: return "none";
        case CompressionAlgorithm::lz4: return "lz4";
    }
    return "unknown";
}


CompressionAlgorithm parseCompressionAlgorithm(std::string_view value)
{
    if (value == "none")
    {
        return CompressionAlgorithm::none;
    }
    if (value == "lz4")
    {
        return CompressionAlgorithm::lz4;
    }
    throw std::invalid_argument(
        "Unsupported storage compression: " + std::string(value)
    );
}


// StoragePolicy implementation
void StoragePolicy::validate() const
{
    if (!enabled)
    {
        return; // Disabled policy, no validation needed
    }

    if (directory.empty())
    {
        throw std::invalid_argument(
            "Storage directory must not be empty"
        );
    }

    if (memory_budget_bytes == 0)
    {
        throw std::invalid_argument(
            "Storage memory budget must be greater than zero"
        );
    }

    if (target_chunk_size_bytes == 0)
    {
        throw std::invalid_argument(
            "Storage target chunk size must be greater than zero"
        );
    }

    if (target_chunk_size_bytes > memory_budget_bytes)
    {
        throw std::invalid_argument(
            "Storage target chunk size (" + 
            formatBytes(target_chunk_size_bytes) + 
            ") must not exceed memory budget (" + 
            formatBytes(memory_budget_bytes) + ")"
        );
    }

    if (max_concurrent_io == 0)
    {
        throw std::invalid_argument(
            "Storage maximum concurrent I/O must be greater than zero"
        );
    }

    if (cache_eviction_threshold < 1 || cache_eviction_threshold > 100)
    {
        throw std::invalid_argument(
            "Cache eviction threshold must be between 1 and 100"
        );
    }

    if (compression != CompressionAlgorithm::none
        && compression != CompressionAlgorithm::lz4)
    {
        throw std::invalid_argument(
            "Unsupported storage compression algorithm"
        );
    }
}


void StoragePolicy::setMemoryBudget(std::uint64_t bytes)
{
    if (bytes == 0)
    {
        throw std::invalid_argument("Memory budget must be greater than zero");
    }
    if (target_chunk_size_bytes > bytes)
    {
        throw std::invalid_argument(
            "Memory budget must be greater than target chunk size"
        );
    }
    memory_budget_bytes = bytes;
}


void StoragePolicy::setTargetChunkSize(std::uint64_t bytes)
{
    if (bytes == 0)
    {
        throw std::invalid_argument("Target chunk size must be greater than zero");
    }
    if (bytes > memory_budget_bytes)
    {
        throw std::invalid_argument(
            "Target chunk size must not exceed memory budget"
        );
    }
    target_chunk_size_bytes = bytes;
}


void StoragePolicy::setCompression(CompressionAlgorithm algo)
{
    if (algo != CompressionAlgorithm::none && algo != CompressionAlgorithm::lz4)
    {
        throw std::invalid_argument("Unsupported compression algorithm");
    }
    compression = algo;
}


void StoragePolicy::setCompression(const std::string& algo)
{
    setCompression(parseCompressionAlgorithm(algo));
}


void StoragePolicy::setMaxConcurrentIo(std::uint32_t ioCount)
{
    if (ioCount == 0)
    {
        throw std::invalid_argument("Max concurrent I/O must be greater than zero");
    }
    max_concurrent_io = ioCount;
}


void StoragePolicy::setCacheEvictionThreshold(std::uint32_t threshold)
{
    if (threshold < 1 || threshold > 100)
    {
        throw std::invalid_argument("Eviction threshold must be between 1 and 100");
    }
    cache_eviction_threshold = threshold;
}


[[nodiscard]] bool StoragePolicy::isEnabled() const noexcept
{
    return enabled;
}


[[nodiscard]] std::filesystem::path StoragePolicy::getDirectory() const noexcept
{
    return directory;
}


[[nodiscard]] std::uint64_t StoragePolicy::getMemoryBudget() const noexcept
{
    return memory_budget_bytes;
}


[[nodiscard]] std::uint64_t StoragePolicy::getTargetChunkSize() const noexcept
{
    return target_chunk_size_bytes;
}


[[nodiscard]] CompressionAlgorithm StoragePolicy::getCompression() const noexcept
{
    return compression;
}


[[nodiscard]] std::uint32_t StoragePolicy::getMaxConcurrentIo() const noexcept
{
    return max_concurrent_io;
}


[[nodiscard]] std::uint32_t StoragePolicy::getCacheEvictionThreshold() const noexcept
{
    return cache_eviction_threshold;
}


// Helper: format bytes to human-readable string
std::string StoragePolicy::formatBytes(std::uint64_t bytes)
{
    const std::uint64_t KB = 1024;
    const std::uint64_t MB = KB * KB;
    const std::uint64_t GB = MB * KB;

    if (bytes >= GB)
    {
        return std::to_string(std::round(bytes / GB)) + " GB";
    }
    else if (bytes >= MB)
    {
        return std::to_string(std::round(bytes / MB)) + " MB";
    }
    else if (bytes >= KB)
    {
        return std::to_string(std::round(bytes / KB)) + " KB";
    }
    else
    {
        return std::to_string(bytes) + " B";
    }
}


std::uint64_t StoragePolicy::parseBytes(const std::string& value)
{
    std::string upperValue = value;
    std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(), ::toupper);

    std::size_t bPos = upperValue.find('B');
    std::size_t kbPos = upperValue.find("KB");
    std::size_t mbPos = upperValue.find("MB");
    std::size_t gbPos = upperValue.find("GB");

    std::uint64_t multiplier = 1;

    if (bPos != std::string::npos)
    {
        multiplier = 1;
    }
    else if (kbPos != std::string::npos)
    {
        multiplier = 1024;
    }
    else if (mbPos != std::string::npos)
    {
        multiplier = 1024 * 1024;
    }
    else if (gbPos != std::string::npos)
    {
        multiplier = 1024 * 1024 * 1024;
    }

    std::string numberStr = value.substr(0, bPos > 0 ? bPos : (kbPos > 0 ? kbPos : (mbPos > 0 ? mbPos : gbPos)));
    
    try
    {
        return std::stoull(numberStr) * multiplier;
    }
    catch (...)
    {
        throw std::invalid_argument("Invalid byte value: " + value);
    }
}


// JSON Serialization (Simplified - no external dependency)
std::string StoragePolicy::toJson() const
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"enabled\": " << (enabled ? "true" : "false") << ",\n";
    oss << "  \"directory\": \"" << directory.string() << "\",\n";
    oss << "  \"memory_budget_bytes\": " << memory_budget_bytes << ",\n";
    oss << "  \"target_chunk_size_bytes\": " << target_chunk_size_bytes << ",\n";
    oss << "  \"compression\": \"" << toString(compression) << "\",\n";
    oss << "  \"max_concurrent_io\": " << max_concurrent_io << ",\n";
    oss << "  \"cache_eviction_threshold\": " << cache_eviction_threshold << "\n";
    oss << "}\n";
    
    return oss.str();
}


std::optional<StoragePolicy> StoragePolicy::fromJson(const std::string& json)
{
    try
    {
        StoragePolicy policy;
        
        // Simple JSON parser for basic structure
        size_t pos = json.find("enabled");
        if (pos != std::string::npos)
        {
            size_t comma = json.find(',', pos);
            if (comma == std::string::npos) comma = json.find('}', pos);
            std::string val = json.substr(pos + 8, comma - pos - 8);
            policy.enabled = (val.find("true") != std::string::npos);
        }
        
        pos = json.find("directory");
        if (pos != std::string::npos)
        {
            size_t start = json.find('"', pos) + 1;
            size_t end = json.find('"', start);
            policy.directory = json.substr(start, end - start);
        }
        
        pos = json.find("memory_budget_bytes");
        if (pos != std::string::npos)
        {
            size_t start = json.find(':', pos);
            size_t end = json.find(',', start);
            if (end == std::string::npos) end = json.find('}', start);
            policy.memory_budget_bytes = std::stoull(json.substr(start + 1, end - start - 1));
        }
        
        pos = json.find("target_chunk_size_bytes");
        if (pos != std::string::npos)
        {
            size_t start = json.find(':', pos);
            size_t end = json.find(',', start);
            if (end == std::string::npos) end = json.find('}', start);
            policy.target_chunk_size_bytes = std::stoull(json.substr(start + 1, end - start - 1));
        }
        
        pos = json.find("compression");
        if (pos != std::string::npos)
        {
            size_t start = json.find('"', pos) + 1;
            size_t end = json.find('"', start);
            policy.compression = parseCompressionAlgorithm(json.substr(start, end - start));
        }
        
        pos = json.find("max_concurrent_io");
        if (pos != std::string::npos)
        {
            size_t start = json.find(':', pos);
            size_t end = json.find(',', start);
            if (end == std::string::npos) end = json.find('}', start);
            policy.max_concurrent_io = std::stoul(json.substr(start + 1, end - start - 1));
        }
        
        pos = json.find("cache_eviction_threshold");
        if (pos != std::string::npos)
        {
            size_t start = json.find(':', pos);
            size_t end = json.find('}', start);
            policy.cache_eviction_threshold = std::stoul(json.substr(start + 1, end - start - 1));
        }
        
        policy.validate();
        return policy;
    }
    catch (...)
    {
        return std::nullopt;
    }
}


// YAML Serialization (Simplified)
std::string StoragePolicy::toYaml() const
{
    std::ostringstream oss;
    oss << "storage:\n";
    oss << "  enabled: " << (enabled ? "true" : "false") << "\n";
    oss << "  directory: " << directory.string() << "\n";
    oss << "  memory_budget: " << formatBytes(memory_budget_bytes) << "\n";
    oss << "  target_chunk_size: " << formatBytes(target_chunk_size_bytes) << "\n";
    oss << "  compression: " << toString(compression) << "\n";
    oss << "  max_concurrent_io: " << max_concurrent_io << "\n";
    oss << "  cache_eviction_threshold: " << cache_eviction_threshold << "%\n";
    
    return oss.str();
}


std::optional<StoragePolicy> StoragePolicy::fromYaml(const std::string& yaml)
{
    try
    {
        StoragePolicy policy;
        bool inStorage = false;
        
        std::istringstream iss(yaml);
        std::string line;
        
        while (std::getline(iss, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            
            if (line.find("storage:") == 0)
            {
                inStorage = true;
                continue;
            }
            
            if (!inStorage)
            {
                continue;
            }
            
            if (line.find("  enabled:") == 0)
            {
                policy.enabled = (line.find("true") != std::string::npos);
            }
            else if (line.find("  directory:") == 0)
            {
                policy.directory = line.substr(12);
            }
            else if (line.find("  memory_budget:") == 0)
            {
                policy.memory_budget_bytes = parseBytes(line.substr(17));
            }
            else if (line.find("  target_chunk_size:") == 0)
            {
                policy.target_chunk_size_bytes = parseBytes(line.substr(21));
            }
            else if (line.find("  compression:") == 0)
            {
                policy.compression = parseCompressionAlgorithm(line.substr(15));
            }
            else if (line.find("  max_concurrent_io:") == 0)
            {
                policy.max_concurrent_io = std::stoi(line.substr(21));
            }
            else if (line.find("  cache_eviction_threshold:") == 0)
            {
                policy.cache_eviction_threshold = std::stoi(line.substr(28));
            }
        }
        
        policy.validate();
        return policy;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

} // namespace pi::storage
