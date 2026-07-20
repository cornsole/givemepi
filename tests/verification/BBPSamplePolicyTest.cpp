#include "verification/BBPSamplePolicy.hpp"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace pi::verification;

    const VerificationIdentity identity{1000, 32, 1032, 73};
    const BBPSamplePolicy policy;
    const auto first = policy.select(identity);
    const auto second = policy.select(identity);
    assert(first == second);
    assert(first.size() == DEFAULT_BBP_SAMPLE_COUNT);
    assert(std::is_sorted(first.begin(), first.end()));
    assert(std::adjacent_find(first.begin(), first.end()) == first.end());
    assert(first.front() == 0);
    assert(first.back() < BBPSamplePolicy::safePositionCount(1000));

    const auto changed = policy.select({1000, 32, 1032, 74});
    assert(changed != first);

    assert(policy.select({1, 1, 2, 1}).empty());
    assert(policy.select({2, 1, 3, 1}).empty());
    const auto tiny = policy.select({4, 1, 5, 1});
    assert(tiny.size() == 1 && tiny[0] == 0);

    const BBPSamplePolicy limited(3);
    assert(limited.select(identity).size() == 3);

    const auto huge = BBPSamplePolicy::safePositionCount(
        std::numeric_limits<std::uint64_t>::max()
    );
    assert(huge > 0);
    assert(huge < std::numeric_limits<std::uint64_t>::max());

    bool zeroRejected = false;
    bool excessRejected = false;
    try { (void)BBPSamplePolicy(0); }
    catch (const std::invalid_argument&) { zeroRejected = true; }
    try { (void)BBPSamplePolicy(MAXIMUM_BBP_SAMPLE_COUNT + 1); }
    catch (const std::invalid_argument&) { excessRejected = true; }
    assert(zeroRejected && excessRejected);
    return 0;
}
