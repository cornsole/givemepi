#include "verification/CanonicalDecimal.hpp"

#include <cassert>
#include <string>


int main()
{
    using namespace pi::verification;

    const auto accepted = inspectCanonicalDecimal("3.14159", 5);
    assert(accepted.accepted());
    assert(accepted.verification.status() == VerificationStatus::passed);
    assert(accepted.decimal->bytes() == "3.14159");
    assert(accepted.decimal->fractionalDigits() == "14159");
    assert(accepted.decimal->requestedDigits() == 5);

    const auto badPrefix = inspectCanonicalDecimal("4.14159", 5);
    assert(!badPrefix.accepted());
    assert(badPrefix.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputPrefix);
    assert(badPrefix.verification.diagnostics().front().offset == 0);

    const auto missingDot = inspectCanonicalDecimal("31.4159", 5);
    assert(missingDot.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputPrefix);
    assert(missingDot.verification.diagnostics().front().offset == 1);

    const auto tooShort = inspectCanonicalDecimal("3.1415", 5);
    assert(tooShort.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputLength);
    assert(tooShort.verification.diagnostics().front().offset == 6);

    const auto trailingNewline = inspectCanonicalDecimal("3.14159\n", 5);
    assert(trailingNewline.verification.diagnostics().front().reason
        == VerificationReason::trailingData);
    assert(trailingNewline.verification.diagnostics().front().offset == 7);

    std::string nonDigit = "3.14159";
    nonDigit[4] = 'x';
    const auto invalidCharacter = inspectCanonicalDecimal(nonDigit, 5);
    assert(invalidCharacter.verification.diagnostics().front().reason
        == VerificationReason::invalidOutputCharacter);
    assert(invalidCharacter.verification.diagnostics().front().offset == 4);

    const auto zeroDigits = inspectCanonicalDecimal("3.", 0);
    assert(zeroDigits.verification.diagnostics().front().reason
        == VerificationReason::operationFailed);

    const auto sign = inspectCanonicalDecimal("+3.14159", 5);
    const auto whitespace = inspectCanonicalDecimal(" 3.14159", 5);
    const auto exponent = inspectCanonicalDecimal("3.1e000", 5);
    assert(!sign.accepted());
    assert(!whitespace.accepted());
    assert(!exponent.accepted());

    return 0;
}
