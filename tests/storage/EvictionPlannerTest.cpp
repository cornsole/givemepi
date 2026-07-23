#include "storage/ChunkStore.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

namespace
{
template<typename F>
bool rejects(F&& function)
{
    try { function(); } catch (const std::invalid_argument&) { return true; }
    return false;
}
}

int main()
{
    using namespace pi::storage;
    StoragePolicy policy;
    policy.directory = std::filesystem::temp_directory_path() / "givemepi-pr0025-eviction-test";
    policy.memory_budget_bytes = 1024;
    policy.target_chunk_size_bytes = 256;
    ChunkStore store(policy);

    const std::vector<std::pair<ChunkId, std::uint64_t>> residents{
        {"z", 100}, {"a", 200}, {"m", 300}, {"b", 50}
    };
    const std::unordered_map<ChunkId, double> distances{
        {"z", 2.0}, {"a", 2.0}, {"m", 0.0}, {"b", 1.0}
    };
    const auto candidates = store.getEvictionCandidates(residents, distances, policy.memory_budget_bytes);
    if (candidates.size() != 3 || candidates[0].chunkId != "a"
        || candidates[1].chunkId != "z" || candidates[2].chunkId != "b")
    {
        std::cerr << "Eviction candidate ordering is not deterministic\n";
        return 1;
    }

    const std::set<ChunkId> residentSet{"a", "b", "m", "z"};
    const auto plan = store.planEvictions(residents, residentSet, distances, 250);
    if (!plan.budgetSatisfied || plan.evictedBytes != 300
        || plan.candidates.size() != 2 || plan.candidates[0].chunkId != "a"
        || plan.candidates[1].chunkId != "z")
    {
        std::cerr << "Eviction plan did not satisfy the requested bytes\n";
        return 1;
    }

    const auto protectedPlan = store.planEvictions(residents, residentSet, distances, 500);
    if (protectedPlan.budgetSatisfied || protectedPlan.candidates.size() != 3
        || protectedPlan.candidates[0].chunkId == "m")
    {
        std::cerr << "Protected or unsatisfied eviction plan is invalid\n";
        return 1;
    }

    if (!rejects([&] {
            store.getEvictionCandidates({{"a", 1}, {"a", 2}}, distances, 1024);
        })
        || !rejects([&] {
            store.getEvictionCandidates({{"a", 1}}, {{"a", -1.0}}, 1024);
        })
        || !rejects([&] {
            store.getEvictionCandidates({{"a", 1}}, {{"a", std::nan(" ")}}, 1024);
        }))
    {
        std::cerr << "Invalid eviction input was accepted\n";
        return 1;
    }

    MemoryBudget budget;
    budget.addResidentBytes(std::numeric_limits<std::uint64_t>::max());
    budget.addResidentBytes(1);
    if (budget.resident_bytes != std::numeric_limits<std::uint64_t>::max())
    {
        std::cerr << "Memory budget overflow was not saturated\n";
        return 1;
    }

    std::error_code ec;
    std::filesystem::remove_all(policy.directory, ec);
    std::cout << "EvictionPlanner OK\n";
    return 0;
}
