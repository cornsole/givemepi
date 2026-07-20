#include "verification/VerificationManifest.hpp"

#include "checkpoint/CRC32C.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pi::verification
{
namespace
{

constexpr std::array<std::uint8_t, 8> MAGIC{'G','M','P','I','V','F','Y',0};
constexpr std::size_t CHECKSUM_OFFSET = 116;
constexpr std::size_t SAMPLE_SIZE = 16;
constexpr std::uint8_t NO_DIGIT = 0xff;

void u8(std::vector<std::uint8_t>& out, std::uint8_t value) { out.push_back(value); }
void u16(std::vector<std::uint8_t>& out, std::uint16_t value)
{ out.push_back(value >> 8); out.push_back(value); }
void u32(std::vector<std::uint8_t>& out, std::uint32_t value)
{ for (int shift = 24; shift >= 0; shift -= 8) out.push_back(value >> shift); }
void u64(std::vector<std::uint8_t>& out, std::uint64_t value)
{ for (int shift = 56; shift >= 0; shift -= 8) out.push_back(value >> shift); }
void storeU32(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value)
{ for (int shift = 24; shift >= 0; shift -= 8) out[offset++] = value >> shift; }

std::uint8_t statusByte(VerificationStatus status)
{
    switch (status)
    {
        case VerificationStatus::passed: return 0;
        case VerificationStatus::failed: return 1;
        case VerificationStatus::skipped: return 2;
        case VerificationStatus::inconclusive: return 3;
    }
    throw std::invalid_argument("Unknown verification status");
}

VerificationStatus statusFromByte(std::uint8_t value)
{
    switch (value)
    {
        case 0: return VerificationStatus::passed;
        case 1: return VerificationStatus::failed;
        case 2: return VerificationStatus::skipped;
        case 3: return VerificationStatus::inconclusive;
        default: throw std::invalid_argument("Unknown manifest status");
    }
}

void validate(const VerificationManifest& manifest)
{
    if (manifest.identity.requestedDigits == 0
        || manifest.identity.requestedDigits
            > std::numeric_limits<std::uint64_t>::max() - 2
        || manifest.identity.workingDigits < manifest.identity.requestedDigits
        || manifest.identity.termCount == 0
        || manifest.canonicalByteCount != manifest.identity.requestedDigits + 2
        || manifest.verifiedAtUnixSeconds == 0)
    {
        throw std::invalid_argument("Invalid verification manifest identity");
    }
    if (manifest.overallStatus != VerificationStatus::passed)
    {
        throw std::invalid_argument("Only passed verification runs are publishable");
    }
    if (manifest.outputFilename.empty()
        || manifest.outputFilename.size() > 255
        || manifest.outputFilename.find_first_of("/\\") != std::string::npos
        || manifest.outputFilename.find('\0') != std::string::npos)
    {
        throw std::invalid_argument("Manifest output must be a plain filename");
    }
    if (manifest.knownDigitsReferenceVersion == 0
        || manifest.bbpSamples.size() > MAXIMUM_BBP_SAMPLE_COUNT)
    {
        throw std::invalid_argument("Invalid verification manifest metadata");
    }
    std::uint64_t previous = 0;
    bool first = true;
    for (const auto& sample : manifest.bbpSamples)
    {
        if (sample.status != VerificationStatus::passed
            || sample.bbpDigit > 15 || sample.outputDigit > 15
            || sample.bbpDigit != sample.outputDigit
            || (!first && sample.position <= previous))
        {
            throw std::invalid_argument("Invalid verification manifest sample");
        }
        first = false;
        previous = sample.position;
    }
}

std::uint32_t checksum(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < VerificationManifestCodec::version1HeaderSize)
        throw std::invalid_argument("Truncated verification manifest");
    std::uint32_t value = pi::checkpoint::CRC32C::calculate(
        bytes.first(CHECKSUM_OFFSET)
    );
    constexpr std::array<std::uint8_t, 4> zeros{};
    value = pi::checkpoint::CRC32C::calculate(zeros, value);
    return pi::checkpoint::CRC32C::calculate(
        bytes.subspan(CHECKSUM_OFFSET + 4), value
    );
}

class Reader
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::span<const std::uint8_t> take(std::size_t count)
    {
        if (count > bytes_.size() - position_)
            throw std::invalid_argument("Truncated verification manifest");
        auto value = bytes_.subspan(position_, count); position_ += count; return value;
    }
    std::uint8_t u8() { return take(1)[0]; }
    std::uint16_t u16() { auto b=take(2); return (std::uint16_t(b[0])<<8)|b[1]; }
    std::uint32_t u32() { std::uint32_t v=0; for(auto b:take(4)) v=(v<<8)|b; return v; }
    std::uint64_t u64() { std::uint64_t v=0; for(auto b:take(8)) v=(v<<8)|b; return v; }
    std::size_t remaining() const { return bytes_.size() - position_; }
private:
    std::span<const std::uint8_t> bytes_; std::size_t position_ = 0;
};

} // namespace

VerificationManifest VerificationManifest::fromRun(
    const FinalVerificationRun& run,
    std::string outputFilename,
    std::uint64_t canonicalByteCount,
    std::uint64_t verifiedAtUnixSeconds
)
{
    if (run.report.status() != VerificationStatus::passed
        || !run.sha256 || !run.knownDigits || !run.knownDigits->matched())
    {
        throw std::invalid_argument("Verification run is not publishable");
    }
    std::vector<VerificationManifestSample> samples;
    samples.reserve(run.bbpSamples.size());
    for (const auto& sample : run.bbpSamples)
    {
        if (sample.status != VerificationStatus::passed
            || !sample.bbpDigit || !sample.outputDigit)
            throw std::invalid_argument("Verification run has unresolved BBP samples");
        samples.push_back({sample.position, sample.status,
            *sample.bbpDigit, *sample.outputDigit});
    }
    VerificationManifest manifest{
        run.report.identity(), std::move(outputFilename), canonicalByteCount,
        *run.sha256, run.knownDigits->referenceVersion, verifiedAtUnixSeconds,
        run.report.status(), std::move(samples)
    };
    validate(manifest);
    return manifest;
}

std::vector<std::uint8_t> VerificationManifestCodec::encode(
    const VerificationManifest& manifest
)
{
    validate(manifest);
    std::vector<std::uint8_t> out;
    out.reserve(version1HeaderSize + manifest.outputFilename.size()
        + manifest.bbpSamples.size() * SAMPLE_SIZE);
    out.insert(out.end(), MAGIC.begin(), MAGIC.end());
    u16(out, formatVersion); u16(out, version1HeaderSize); u32(out, 0);
    u64(out, manifest.identity.requestedDigits); u32(out, manifest.identity.guardDigits); u32(out, 0);
    u64(out, manifest.identity.workingDigits); u64(out, manifest.identity.termCount);
    u64(out, manifest.canonicalByteCount);
    out.insert(out.end(), manifest.sha256.begin(), manifest.sha256.end());
    u32(out, manifest.knownDigitsReferenceVersion);
    u8(out, statusByte(manifest.overallStatus)); u8(out, 0); u16(out, 0);
    u64(out, manifest.verifiedAtUnixSeconds);
    u32(out, static_cast<std::uint32_t>(manifest.bbpSamples.size()));
    u32(out, static_cast<std::uint32_t>(manifest.outputFilename.size()));
    u16(out, 1); u16(out, 4); u32(out, 0);
    out.insert(out.end(), manifest.outputFilename.begin(), manifest.outputFilename.end());
    for (const auto& sample : manifest.bbpSamples)
    {
        u64(out, sample.position); u8(out, statusByte(sample.status));
        u8(out, sample.bbpDigit); u8(out, sample.outputDigit); u8(out, NO_DIGIT);
        u32(out, 0);
    }
    storeU32(out, CHECKSUM_OFFSET, checksum(out));
    return out;
}

VerificationManifest VerificationManifestCodec::decode(
    std::span<const std::uint8_t> bytes
)
{
    Reader reader(bytes);
    const auto magic = reader.take(8);
    if (!std::equal(magic.begin(), magic.end(), MAGIC.begin()))
        throw std::invalid_argument("Invalid verification manifest magic");
    if (reader.u16()!=formatVersion || reader.u16()!=version1HeaderSize || reader.u32()!=0)
        throw std::invalid_argument("Unsupported verification manifest version");
    VerificationIdentity identity{reader.u64(), reader.u32(), 0, 0};
    if (reader.u32()!=0) throw std::invalid_argument("Non-zero reserved field");
    identity.workingDigits=reader.u64(); identity.termCount=reader.u64();
    const auto canonicalBytes=reader.u64();
    SHA256Digest digest{}; const auto digestBytes=reader.take(digest.size());
    std::copy(digestBytes.begin(), digestBytes.end(), digest.begin());
    const auto referenceVersion=reader.u32();
    const auto overall=statusFromByte(reader.u8());
    if (reader.u8()!=0 || reader.u16()!=0) throw std::invalid_argument("Non-zero reserved field");
    const auto timestamp=reader.u64(); const auto sampleCount=reader.u32();
    const auto filenameLength=reader.u32();
    if (reader.u16()!=1 || reader.u16()!=4) throw std::invalid_argument("Unsupported manifest checksum");
    const auto storedChecksum=reader.u32();
    if (storedChecksum != checksum(bytes)) throw std::invalid_argument("Verification manifest CRC32C mismatch");
    if (sampleCount > MAXIMUM_BBP_SAMPLE_COUNT || filenameLength > 255
        || reader.remaining() != filenameLength + std::size_t(sampleCount) * SAMPLE_SIZE)
        throw std::invalid_argument("Invalid verification manifest lengths");
    const auto filenameBytes=reader.take(filenameLength);
    std::string filename(filenameBytes.begin(), filenameBytes.end());
    std::vector<VerificationManifestSample> samples; samples.reserve(sampleCount);
    for (std::uint32_t index=0; index<sampleCount; ++index)
    {
        const auto position=reader.u64(); const auto status=statusFromByte(reader.u8());
        const auto bbp=reader.u8(); const auto output=reader.u8();
        if (reader.u8()!=NO_DIGIT || reader.u32()!=0) throw std::invalid_argument("Non-zero sample reserved field");
        samples.push_back({position,status,bbp,output});
    }
    VerificationManifest manifest{identity,std::move(filename),canonicalBytes,digest,
        referenceVersion,timestamp,overall,std::move(samples)};
    validate(manifest); return manifest;
}

void VerificationManifestStore::save(
    const std::filesystem::path& destination,
    const VerificationManifest& manifest,
    pi::checkpoint::AtomicCommitObserver observer
)
{
    const auto bytes=VerificationManifestCodec::encode(manifest);
    pi::checkpoint::AtomicFileCommit::write(destination, bytes, observer);
}

VerificationManifest VerificationManifestStore::load(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Cannot open verification manifest");
    const auto end=input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > 4096)
        throw std::runtime_error("Invalid verification manifest file size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end)); input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input && !bytes.empty()) throw std::runtime_error("Cannot read verification manifest");
    return VerificationManifestCodec::decode(bytes);
}

} // namespace pi::verification
