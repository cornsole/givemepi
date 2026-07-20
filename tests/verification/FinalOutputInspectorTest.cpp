#include "verification/FinalOutputInspector.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>


namespace
{

class TemporaryFile
{
public:
    explicit TemporaryFile(std::string content)
        : path_(std::filesystem::temp_directory_path()
            / ("givemepi-output-inspection-"
                + std::to_string(::getpid()) + '-'
                + std::to_string(sequence_++)))
    {
        std::ofstream output(path_, std::ios::binary);
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    ~TemporaryFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    inline static unsigned sequence_ = 0;
    std::filesystem::path path_;
};

} // namespace


int main()
{
    using namespace pi::verification;

    const auto memory = FinalOutputInspector::inspectMemory("3.14159", 5);
    assert(memory.accepted());

    TemporaryFile valid("3.14159\n");
    const auto accepted = FinalOutputInspector::inspectFile(valid.path(), 5);
    assert(accepted.accepted());
    assert(accepted.file->path == valid.path());
    assert(accepted.file->fileSize == 8);
    assert(accepted.file->canonicalByteCount == 7);
    assert(accepted.file->requestedDigits == 5);

    TemporaryFile noNewline("3.14159");
    const auto shortFile = FinalOutputInspector::inspectFile(noNewline.path(), 5);
    assert(shortFile.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputLength);
    assert(shortFile.verification.diagnostics().front().offset == 7);

    TemporaryFile crlf("3.14159\r\n");
    const auto wrongTerminator = FinalOutputInspector::inspectFile(crlf.path(), 5);
    assert(wrongTerminator.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputCharacter);
    assert(wrongTerminator.verification.diagnostics().front().offset == 7);

    TemporaryFile trailing("3.14159\nextra");
    const auto trailingData = FinalOutputInspector::inspectFile(trailing.path(), 5);
    assert(trailingData.verification.diagnostics().front().reason
        == VerificationReason::trailingData);
    assert(trailingData.verification.diagnostics().front().offset == 8);

    TemporaryFile invalidDigit("3.14x59\n");
    const auto badDigit = FinalOutputInspector::inspectFile(invalidDigit.path(), 5);
    assert(badDigit.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputCharacter);
    assert(badDigit.verification.diagnostics().front().offset == 4);

    std::string large = "3." + std::string(200'000, '1') + '\n';
    TemporaryFile largeFile(std::move(large));
    const auto largeAccepted = FinalOutputInspector::inspectFile(
        largeFile.path(),
        200'000
    );
    assert(largeAccepted.accepted());
    assert(largeAccepted.file->fileSize == 200'003);

    const auto missing = FinalOutputInspector::inspectFile(
        valid.path().string() + ".missing",
        5
    );
    assert(missing.verification.diagnostics().front().reason
        == VerificationReason::fileReadFailed);

    return 0;
}
