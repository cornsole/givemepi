#pragma once

#include "checkpoint/CheckpointTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <set>
#include <unordered_map>


namespace pi::storage
{

// Chunk ID 타입 - hex string
using ChunkId = std::string;

enum class CompressionAlgorithm : std::uint16_t
{
    none = 0,
    lz4 = 1
};


enum class ChunkChecksumAlgorithm : std::uint16_t
{
    crc32c = 1
};


inline constexpr std::uint16_t chunkFormatVersion = 1;
inline constexpr std::uint64_t maxChunkPayloadBytes = 1ULL << 30;


struct ChunkIdentity
{
    checkpoint::ComputationIdentity computation;
    checkpoint::BlockLocation location;

    [[nodiscard]]
    static ChunkIdentity create(
        const checkpoint::ComputationIdentity& computation,
        const checkpoint::BlockLocation& location
    );

    void validate() const;

    [[nodiscard]]
    std::string deterministicFilename() const;

    [[nodiscard]]
    bool operator==(const ChunkIdentity&) const noexcept = default;

    [[nodiscard]]
    bool operator<(const ChunkIdentity& other) const noexcept {
        if (computation != other.computation) {
            return computation < other.computation;
        }
        return location < other.location;
    }
};


struct ChunkMetadata
{
    ChunkIdentity identity;
    std::uint64_t uncompressedSize;
    std::uint64_t storedSize;
    ChunkChecksumAlgorithm checksumAlgorithm;
    std::uint32_t checksumValue;
    CompressionAlgorithm compression;
    std::uint16_t formatVersion;

    [[nodiscard]]
    static ChunkMetadata create(
        ChunkIdentity identity,
        std::uint64_t uncompressedSize,
        std::uint64_t storedSize,
        ChunkChecksumAlgorithm checksumAlgorithm,
        std::uint32_t checksumValue,
        CompressionAlgorithm compression = CompressionAlgorithm::none,
        std::uint16_t formatVersion = chunkFormatVersion
    );

    void validate() const;

    [[nodiscard]]
    static std::string createChunkFilename(ChunkId chunkId);

    [[nodiscard]]
    bool operator==(const ChunkMetadata&) const noexcept = default;
};


/**
 * @brief 메모리 예산 추적 정보
 * 
 * resident bytes: 현재 메모리에 resident 된 chunk 의 uncompressed size 총합
 * storage bytes: disk 에 저장된 chunk 의 stored size 총합
 * 
 * eviction planner 가 budget 에 따라 결정
 */
struct MemoryBudget
{
    std::uint64_t resident_bytes = 0;    // 메모리 내 uncompressed size
    std::uint64_t storage_bytes = 0;     // 디스크에 저장된 stored size
    std::uint64_t budget_bytes = 0;       // 할당된 메모리 예산
    std::uint64_t target_chunk_size = 0;  // 타겟 chunk 크기

    [[nodiscard]]
    double budgetUtilization() const noexcept;

    [[nodiscard]]
    double storageUtilization() const noexcept;

    [[nodiscard]]
    std::uint64_t availableBudget() const noexcept;

    /**
     * @brief resident bytes 증가
     * @param bytes 증가할 uncompressed bytes
     */
    void addResidentBytes(std::uint64_t bytes);

    /**
     * @brief resident bytes 감소
     * @param bytes 감소할 uncompressed bytes
     */
    void removeResidentBytes(std::uint64_t bytes);

    /**
     * @brief storage bytes 증가
     * @param bytes 증가할 stored bytes
     */
    void addStorageBytes(std::uint64_t bytes);

    /**
     * @brief storage bytes 감소
     * @param bytes 감소할 stored bytes
     */
    void removeStorageBytes(std::uint64_t bytes);

    /**
     * @brief 예산 설정
     */
    void setBudget(std::uint64_t bytes);

    /**
     * @brief 타겟 chunk 크기 설정
     */
    void setTargetChunkSize(std::uint64_t bytes);

    /**
     * @brief 예산 리셋
     */
    void reset() noexcept;

    /**
     * @brief 문자열로 표현
     */
    [[nodiscard]]
    std::string toString() const;

private:
    static constexpr double storageUtilizationThreshold = 0.80; // 80% 초과 시 eviction 고려
};


/**
 * @brief eviction 후보 정보
 * 
 * 어떤 chunk 를 evict 해야 할지 선택하는 정보를 담음
 * 
 * evictionScore: eviction 우선순위 점수 (높을수록 우선 eviction)
 * mergeDistance: 해당 chunk 가 다음 merge 에 얼마나 필요한지 (높을수록 eviction 금지)
 */
struct EvictionCandidate
{
    ChunkId chunkId;
    std::uint64_t uncompressedSize;  // uncompressed size
    std::uint64_t storedSize;        // stored size
    double evictionScore;            // eviction 우선순위 점수
    double mergeDistance;            // 다음 merge 까지 거리 (0 = 즉시 필요)
    std::optional<std::string> reason; // eviction 이유

    [[nodiscard]]
    bool operator<(const EvictionCandidate& other) const noexcept {
        if (mergeDistance != other.mergeDistance)
            return mergeDistance > other.mergeDistance;
        if (uncompressedSize != other.uncompressedSize)
            return uncompressedSize > other.uncompressedSize;
        return chunkId < other.chunkId;
    }
};


/**
 * @brief eviction 계획 결과
 * 
 * budget 을 만족시키기 위해 evict 해야 하는 chunk 목록
 */
struct EvictionPlan
{
    std::vector<EvictionCandidate> candidates;
    std::uint64_t evictedBytes = 0;    // evicted uncompressed bytes
    std::uint64_t evictedStoredBytes = 0; // evicted stored bytes
    bool budgetSatisfied = false;      // 예산 충족 여부
    std::optional<std::string> reason; // 실패 이유

    /**
     * @brief 예산 충족 여부 업데이트
     */
    void updateBudgetStatus(std::uint64_t currentBudget, std::uint64_t budget);

    /**
     * @brief budget 이 충족될 때까지 evict
     * @param neededBytes 추가로 evict 해야 하는 bytes
     */
    void evictUntilBudgetSatisfied(std::uint64_t neededBytes);

    /**
     * @brief 문자열로 표현
     */
    [[nodiscard]]
    std::string toString() const;
};


} // namespace pi::storage
