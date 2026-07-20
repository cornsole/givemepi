#include "verification/KnownDigitsVerifier.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>


int main()
{
    using namespace pi::verification;

    const auto reference = KnownDigitsVerifier::referenceFractionalDigits();
    assert(reference.size() == 100);

    for (const std::size_t digits : {1U, 10U, 50U, 100U})
    {
        const std::string rounded = digits == 1 ? "1"
            : digits == 10 ? "1415926536"
            : digits == 50
                ? "14159265358979323846264338327950288419716939937511"
                : "14159265358979323846264338327950288419716939937510"
                  "58209749445923078164062862089986280348253421170680";
        auto inspected = inspectCanonicalDecimal(
            "3." + rounded,
            digits
        );
        assert(inspected.accepted());
        const auto result = KnownDigitsVerifier::verify(*inspected.decimal);
        assert(result.matched());
        assert(result.referenceVersion == 1);
        assert(result.referenceDigits == 100);
        assert(result.comparedDigits == digits);
    }

    auto longer = inspectCanonicalDecimal(
        "3." + std::string(reference) + std::string(25, '7'),
        125
    );
    assert(longer.accepted());
    const auto bounded = KnownDigitsVerifier::verify(*longer.decimal);
    assert(bounded.matched());
    assert(bounded.comparedDigits == 100);

    std::string incorrect(reference.substr(0, 50));
    incorrect[37] = incorrect[37] == '0' ? '1' : '0';
    auto mismatchInput = inspectCanonicalDecimal("3." + incorrect, 50);
    assert(mismatchInput.accepted());
    const auto mismatch = KnownDigitsVerifier::verify(*mismatchInput.decimal);
    assert(!mismatch.matched());
    assert(mismatch.verification.diagnostics().size() == 1);
    assert(mismatch.verification.diagnostics()[0].reason
        == VerificationReason::knownDigitsMismatch);
    assert(mismatch.verification.diagnostics()[0].offset == 39);

    const auto path = std::filesystem::temp_directory_path()
        / ("pi-known-digits-" + std::to_string(::getpid()) + ".txt");
    {
        std::ofstream output(path, std::ios::binary);
        output << "3.14159265358979323846264338327950288419716939937510"
                  "58209749445923078164062862089986280348253421170680\n";
    }
    const auto fileInspection = FinalOutputInspector::inspectFile(path, 100);
    assert(fileInspection.accepted());
    const auto fileResult = KnownDigitsVerifier::verify(*fileInspection.file);
    assert(fileResult.matched());
    assert(fileResult.comparedDigits == 100);

    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << "3.0";
    }
    const auto changed = KnownDigitsVerifier::verify(*fileInspection.file);
    assert(!changed.matched());
    assert(changed.verification.diagnostics()[0].reason
        == VerificationReason::fileReadFailed);
    std::filesystem::remove(path);

    return 0;
}
