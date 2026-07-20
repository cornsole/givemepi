#include "checkpoint/CheckpointValidation.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>


using pi::checkpoint::RejectionReason;
using pi::checkpoint::ValidationDiagnostic;
using pi::checkpoint::ValidationResult;
using pi::checkpoint::ValidationStage;
using pi::checkpoint::ValidationStatus;
using pi::checkpoint::toString;


int main()
{
    const ValidationResult accepted = ValidationResult::accepted();
    if (!accepted.isAccepted() || accepted.isRejected() || accepted.isIgnored()
        || !accepted.diagnostics().empty()
        || accepted.status() != ValidationStatus::accepted)
    {
        std::cerr << "Accepted validation result mismatch\n";
        return 1;
    }

    const ValidationDiagnostic checksumDiagnostic{
        ValidationStage::checksum,
        RejectionReason::checksumMismatch,
        "block CRC32C differs"
    };
    const ValidationResult rejected =
        ValidationResult::rejected(checksumDiagnostic);

    if (!rejected.isRejected() || rejected.diagnostics().size() != 1
        || rejected.diagnostics().front() != checksumDiagnostic)
    {
        std::cerr << "Rejected validation result mismatch\n";
        return 1;
    }

    const ValidationResult multiple = ValidationResult::rejected({
        checksumDiagnostic,
        ValidationDiagnostic{
            ValidationStage::manifest,
            RejectionReason::sizeMismatch,
            "manifest payload size differs"
        }
    });

    if (multiple.diagnostics().size() != 2)
    {
        std::cerr << "Multiple validation diagnostics were not preserved\n";
        return 1;
    }

    const ValidationResult ignored = ValidationResult::ignored(
        RejectionReason::temporaryFile,
        "unfinished temp file"
    );
    if (!ignored.isIgnored() || ignored.diagnostics().size() != 1
        || ignored.diagnostics().front().stage != ValidationStage::discovery)
    {
        std::cerr << "Ignored validation result mismatch\n";
        return 1;
    }

    bool rejectedEmptyDiagnostics = false;
    try
    {
        static_cast<void>(ValidationResult::rejected(
            std::vector<ValidationDiagnostic>{}
        ));
    }
    catch (const std::invalid_argument&)
    {
        rejectedEmptyDiagnostics = true;
    }

    if (!rejectedEmptyDiagnostics
        || toString(ValidationStatus::rejected) != "rejected"
        || toString(ValidationStage::mathematics) != "mathematics"
        || toString(RejectionReason::modularMismatch) != "modular_mismatch")
    {
        std::cerr << "Validation diagnostic invariant mismatch\n";
        return 1;
    }

    std::cout << "CheckpointValidation OK\n";
    return 0;
}
