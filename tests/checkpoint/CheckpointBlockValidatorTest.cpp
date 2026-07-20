#include "checkpoint/CheckpointBlockValidator.hpp"

#include "checkpoint/CRC32C.hpp"
#include "chudnovsky/PrecisionPolicy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>


using pi::bigint::GMPInteger;
using pi::checkpoint::BlockLocation;
using pi::checkpoint::CheckpointBlock;
using pi::checkpoint::CheckpointBlockCodec;
using pi::checkpoint::CheckpointBlockValidation;
using pi::checkpoint::CheckpointBlockValidator;
using pi::checkpoint::ComputationIdentity;
using pi::checkpoint::CRC32C;
using pi::checkpoint::RejectionReason;
using pi::chudnovsky::PrecisionPolicy;


namespace
{

void refreshChecksum(std::vector<std::uint8_t>& bytes)
{
    for (std::size_t offset = 80; offset < 84; ++offset) bytes[offset] = 0;
    const std::uint32_t checksum = CRC32C::calculate(bytes);
    for (int shift = 24, offset = 80; shift >= 0; shift -= 8, ++offset)
        bytes[offset] = static_cast<std::uint8_t>(checksum >> shift);
}


bool hasReason(
    const CheckpointBlockValidation& validation,
    RejectionReason reason
)
{
    return !validation.validation.diagnostics().empty()
        && validation.validation.diagnostics().front().reason == reason;
}

} // namespace


int main()
{
    const ComputationIdentity identity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(1000));
    GMPInteger negative(128);
    negative.negate();
    const CheckpointBlock block{
        identity,
        BlockLocation::create(0, 10, 3, identity),
        GMPInteger(256),
        GMPInteger(1000),
        negative
    };
    const std::vector<std::uint8_t> encoded =
        CheckpointBlockCodec::encode(block);

    const CheckpointBlockValidation accepted =
        CheckpointBlockValidator::validate(encoded, identity);
    if (!accepted.hasAcceptedBlock()
        || accepted.block->block.location != block.location
        || accepted.block->encodedSize != encoded.size())
    {
        std::cerr << "Valid block was not accepted\n";
        return 1;
    }

    std::vector<std::uint8_t> invalidRange = encoded;
    std::copy(invalidRange.begin() + 64, invalidRange.begin() + 72,
              invalidRange.begin() + 56);
    refreshChecksum(invalidRange);

    std::vector<std::uint8_t> invalidLevel = encoded;
    std::fill(invalidLevel.begin() + 72, invalidLevel.begin() + 76, 0xFF);
    refreshChecksum(invalidLevel);

    std::vector<std::uint8_t> invalidLength = encoded;
    ++invalidLength[95];
    refreshChecksum(invalidLength);

    std::vector<std::uint8_t> trailing = encoded;
    trailing.push_back(0);
    refreshChecksum(trailing);

    std::vector<std::uint8_t> nonCanonical = encoded;
    nonCanonical[CheckpointBlockCodec::version1HeaderSize] = 0;
    refreshChecksum(nonCanonical);

    std::vector<std::uint8_t> badChecksum = encoded;
    badChecksum.back() ^= 1;

    const ComputationIdentity otherIdentity =
        ComputationIdentity::fromPrecisionPlan(PrecisionPolicy::create(2000));

    if (!hasReason(CheckpointBlockValidator::validate(invalidRange, identity),
                   RejectionReason::invalidRange)
        || !hasReason(CheckpointBlockValidator::validate(invalidLevel, identity),
                      RejectionReason::invalidTreeLevel)
        || !hasReason(CheckpointBlockValidator::validate(invalidLength, identity),
                      RejectionReason::invalidPayloadLength)
        || !hasReason(CheckpointBlockValidator::validate(trailing, identity),
                      RejectionReason::trailingBytes)
        || !hasReason(CheckpointBlockValidator::validate(nonCanonical, identity),
                      RejectionReason::nonCanonicalInteger)
        || !hasReason(CheckpointBlockValidator::validate(badChecksum, identity),
                      RejectionReason::checksumMismatch)
        || !hasReason(CheckpointBlockValidator::validate(encoded, otherIdentity),
                      RejectionReason::invalidIdentity))
    {
        std::cerr << "Block rejection reason mismatch\n";
        return 1;
    }

    std::cout << "CheckpointBlockValidator OK\n";
    return 0;
}
