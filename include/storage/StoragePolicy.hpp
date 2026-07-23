#pragma once

#include "storage/ChunkTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>


namespace pi::storage
{

// 저장소 정책 관련 상수들
inline constexpr std::uint64_t defaultMemoryBudgetBytes = 512ULL * 1024ULL * 1024ULL; // 512MB
inline constexpr std::uint64_t defaultTargetChunkSizeBytes = 64ULL * 1024ULL * 1024ULL; // 64MB
inline constexpr std::uint32_t defaultMaxConcurrentIo = 1;
inline constexpr std::uint32_t defaultCacheEvictionThreshold = 80; // 80% 사용 시 evict


// 압축 알고리즘 문자열 ↔ enum 변환
[[nodiscard]]
std::string_view toString(CompressionAlgorithm compression) noexcept;


[[nodiscard]]
CompressionAlgorithm parseCompressionAlgorithm(std::string_view value);


/**
 * @brief 저장소 설정 정책 (Storage Policy)
 * 
 * 스토리지 시스템의 동작을 제어하는 정책 설정입니다.
 * 메모리 예산, 압축, I/O 제한 등을 관리합니다.
 */
struct StoragePolicy
{
    // 기본 설정값들
    bool enabled = true;
    std::filesystem::path directory = "storage";
    std::uint64_t memory_budget_bytes = defaultMemoryBudgetBytes;
    std::uint64_t target_chunk_size_bytes = defaultTargetChunkSizeBytes;
    CompressionAlgorithm compression = CompressionAlgorithm::none;
    std::uint32_t max_concurrent_io = defaultMaxConcurrentIo;
    std::uint32_t cache_eviction_threshold = defaultCacheEvictionThreshold;

    // 생성자
    // StoragePolicy() = default; // Removed for default initialization

    /**
     * @brief 기본 생성자
     */
    explicit StoragePolicy(
        bool enable = true,
        const std::filesystem::path& dir = "storage",
        std::uint64_t budget = defaultMemoryBudgetBytes,
        std::uint64_t targetSize = defaultTargetChunkSizeBytes,
        CompressionAlgorithm algo = CompressionAlgorithm::none,
        std::uint32_t maxIo = defaultMaxConcurrentIo,
        std::uint32_t evictionThreshold = defaultCacheEvictionThreshold
    )
        : enabled(enable)
        , directory(dir)
        , memory_budget_bytes(budget)
        , target_chunk_size_bytes(targetSize)
        , compression(algo)
        , max_concurrent_io(maxIo)
        , cache_eviction_threshold(evictionThreshold)
    {
    }

    // 복사/이동 생성자
    StoragePolicy(const StoragePolicy&) = default;
    StoragePolicy& operator=(const StoragePolicy&) = default;
    StoragePolicy(StoragePolicy&&) noexcept = default;
    StoragePolicy& operator=(StoragePolicy&&) noexcept = default;

    /**
     * @brief 설정 유효성 검사
     * @throws std::invalid_argument 유효하지 않은 설정일 경우
     */
    void validate() const;

    /**
     * @brief 메모리 예산 설정
     */
    void setMemoryBudget(std::uint64_t bytes);

    /**
     * @brief 타겟 chunk 크기 설정
     */
    void setTargetChunkSize(std::uint64_t bytes);

    /**
     * @brief 압축 알고리즘 설정
     */
    void setCompression(CompressionAlgorithm algo);

    /**
     * @brief 압축 알고리즘 설정 (문자열)
     */
    void setCompression(const std::string& algo);

    /**
     * @brief 최대 동시 I/O 설정
     */
    void setMaxConcurrentIo(std::uint32_t ioCount);

    /**
     * @brief 캐시 eviction 임계값 설정 (%)
     */
    void setCacheEvictionThreshold(std::uint32_t threshold);

    // Getter 메서드들
    [[nodiscard]] bool isEnabled() const noexcept;
    [[nodiscard]] std::filesystem::path getDirectory() const noexcept;
    [[nodiscard]] std::uint64_t getMemoryBudget() const noexcept;
    [[nodiscard]] std::uint64_t getTargetChunkSize() const noexcept;
    [[nodiscard]] CompressionAlgorithm getCompression() const noexcept;
    [[nodiscard]] std::uint32_t getMaxConcurrentIo() const noexcept;
    [[nodiscard]] std::uint32_t getCacheEvictionThreshold() const noexcept;

    /**
     * @brief 설정을 JSON 문자열로 직렬화
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief JSON 문자열에서 설정을 역직렬화
     */
    [[nodiscard]] static std::optional<StoragePolicy> fromJson(const std::string& json);

    /**
     * @brief 설정을 YAML 문자열로 직렬화
     */
    [[nodiscard]] std::string toYaml() const;

    /**
     * @brief YAML 문자열에서 설정을 역직렬화
     */
    [[nodiscard]] static std::optional<StoragePolicy> fromYaml(const std::string& yaml);

private:
    /**
     * @brief 단위 변환 헬퍼 (bytes → MB, GB 등)
     */
    static std::string formatBytes(std::uint64_t bytes);

    /**
     * @brief 단위 변환 헬퍼 (MB → bytes 등)
     */
    static std::uint64_t parseBytes(const std::string& value);
};

} // namespace pi::storage
