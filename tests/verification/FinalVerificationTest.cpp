#include "verification/FinalVerification.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <utility>
#include <vector>


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
    using namespace std::chrono_literals;
    using namespace pi::verification;

    const VerificationIdentity identity{1'000, 32, 1'032, 74};
    const VerificationCheckResult structure(
        VerificationStage::outputStructure,
        VerificationStatus::passed,
        10ns
    );
    const VerificationCheckResult hash(
        VerificationStage::hash,
        VerificationStatus::passed,
        20ns
    );
    const FinalVerificationReport passed(identity, {structure, hash});
    assert(passed.schemaVersion() == 1);
    assert(passed.identity() == identity);
    assert(passed.status() == VerificationStatus::passed);
    assert(passed.checks().size() == 2);
    assert(passed.find(VerificationStage::hash) != nullptr);
    assert(passed.find(VerificationStage::bbp) == nullptr);

    const VerificationDiagnostic uncertain{
        VerificationStage::bbp,
        VerificationReason::insufficientPrecision,
        "Sample is too close to a hexadecimal boundary",
        500
    };
    const VerificationCheckResult bbp(
        VerificationStage::bbp,
        VerificationStatus::inconclusive,
        30ns,
        {uncertain}
    );
    const FinalVerificationReport inconclusive(
        identity,
        {structure, bbp}
    );
    assert(inconclusive.status() == VerificationStatus::inconclusive);
    assert(inconclusive.find(VerificationStage::bbp)->diagnostics().front()
        == uncertain);

    const VerificationDiagnostic mismatch{
        VerificationStage::knownDigits,
        VerificationReason::knownDigitsMismatch,
        "Known prefix differs",
        42
    };
    const VerificationCheckResult known(
        VerificationStage::knownDigits,
        VerificationStatus::failed,
        40ns,
        {mismatch}
    );
    const VerificationCheckResult skipped(
        VerificationStage::manifest,
        VerificationStatus::skipped,
        0ns
    );
    const FinalVerificationReport failed(
        identity,
        {skipped, bbp, known}
    );
    assert(failed.status() == VerificationStatus::failed);

    const FinalVerificationReport passedWithSkipped(
        identity,
        {
            skipped,
            VerificationCheckResult(
                VerificationStage::hash,
                VerificationStatus::passed,
                1ns
            )
        }
    );
    assert(passedWithSkipped.status() == VerificationStatus::passed);

    assert(throws<std::invalid_argument>([&mismatch]() {
        static_cast<void>(VerificationCheckResult(
            VerificationStage::knownDigits,
            VerificationStatus::passed,
            0ns,
            {mismatch}
        ));
    }));
    assert(throws<std::invalid_argument>([]() {
        static_cast<void>(VerificationCheckResult(
            VerificationStage::hash,
            VerificationStatus::failed,
            0ns
        ));
    }));
    assert(throws<std::invalid_argument>([&mismatch]() {
        static_cast<void>(VerificationCheckResult(
            VerificationStage::hash,
            VerificationStatus::failed,
            0ns,
            {mismatch}
        ));
    }));
    assert(throws<std::invalid_argument>([]() {
        static_cast<void>(VerificationCheckResult(
            VerificationStage::hash,
            VerificationStatus::passed,
            -1ns
        ));
    }));
    assert(throws<std::invalid_argument>([&identity, &structure]() {
        static_cast<void>(FinalVerificationReport(
            identity,
            {structure, structure}
        ));
    }));
    assert(throws<std::invalid_argument>([&identity]() {
        static_cast<void>(FinalVerificationReport(identity, {}));
    }));
    assert(throws<std::invalid_argument>([&structure]() {
        static_cast<void>(FinalVerificationReport(
            VerificationIdentity{0, 32, 32, 1},
            {structure}
        ));
    }));

    assert(toString(VerificationStatus::inconclusive) == "inconclusive");
    assert(toString(VerificationStage::outputStructure)
        == "output_structure");
    assert(toString(VerificationReason::bbpMismatch) == "bbp_mismatch");
    assert(toString(static_cast<VerificationStatus>(255)) == "unknown");

    return 0;
}
