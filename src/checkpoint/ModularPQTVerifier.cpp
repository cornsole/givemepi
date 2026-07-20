#include "checkpoint/ModularPQTVerifier.hpp"

#include <exception>
#include <string>
#include <utility>
#include <vector>


namespace pi::checkpoint
{

namespace
{

constexpr std::uint64_t qConstant = 10939058860032000ULL;
constexpr std::uint64_t tMultiplier = 545140134ULL;
constexpr std::uint64_t tConstant = 13591409ULL;


struct Residues
{
    std::uint64_t p;
    std::uint64_t q;
    std::uint64_t t;
};


std::uint64_t multiply(
    std::uint64_t left,
    std::uint64_t right,
    std::uint64_t prime
) noexcept
{
    return (left * right) % prime;
}


std::uint64_t subtractSmall(
    std::uint64_t value,
    std::uint64_t amount,
    std::uint64_t prime
) noexcept
{
    return (value + prime - amount) % prime;
}


Residues leaf(std::uint64_t term, std::uint64_t prime) noexcept
{
    if (term == 0)
    {
        return Residues{1, 1, tConstant % prime};
    }

    const std::uint64_t k = term % prime;
    const std::uint64_t first = subtractSmall(
        multiply(6, k, prime), 5, prime
    );
    const std::uint64_t second = subtractSmall(
        multiply(2, k, prime), 1, prime
    );
    const std::uint64_t third = subtractSmall(
        multiply(6, k, prime), 1, prime
    );
    const std::uint64_t p = multiply(
        multiply(first, second, prime), third, prime
    );
    const std::uint64_t kSquared = multiply(k, k, prime);
    const std::uint64_t q = multiply(
        multiply(kSquared, k, prime), qConstant % prime, prime
    );
    const std::uint64_t linear = (
        multiply(k, tMultiplier % prime, prime) + tConstant % prime
    ) % prime;
    std::uint64_t t = multiply(linear, p, prime);

    if ((term & 1U) != 0 && t != 0)
    {
        t = prime - t;
    }

    return Residues{p, q, t};
}


Residues merge(
    const Residues& left,
    const Residues& right,
    std::uint64_t prime
) noexcept
{
    return Residues{
        multiply(left.p, right.p, prime),
        multiply(left.q, right.q, prime),
        (
            multiply(left.t, right.q, prime)
            + multiply(left.p, right.t, prime)
        ) % prime
    };
}


Residues calculate(
    std::uint64_t start,
    std::uint64_t end,
    std::uint64_t prime
)
{
    if (end - start == 1)
    {
        return leaf(start, prime);
    }

    const std::uint64_t middle = start + (end - start) / 2;
    return merge(
        calculate(start, middle, prime),
        calculate(middle, end, prime),
        prime
    );
}


std::uint64_t residue(
    const bigint::GMPInteger& value,
    std::uint32_t prime
) noexcept
{
    return mpz_fdiv_ui(*value.raw(), static_cast<unsigned long>(prime));
}

} // namespace


ValidationResult ModularPQTVerifier::validate(const CheckpointBlock& block)
{
    try
    {
        block.identity.validate();
        block.location.validate(block.identity);
    }
    catch (const std::exception& error)
    {
        return ValidationResult::rejected(ValidationDiagnostic{
            ValidationStage::mathematics,
            RejectionReason::invalidRange,
            error.what()
        });
    }

    std::vector<ValidationDiagnostic> diagnostics;
    for (const std::uint32_t prime : verificationPrimes)
    {
        const Residues expected = calculate(
            block.location.start,
            block.location.end,
            prime
        );
        std::string mismatches;

        if (residue(block.p, prime) != expected.p) mismatches += "P";
        if (residue(block.q, prime) != expected.q) mismatches += "Q";
        if (residue(block.t, prime) != expected.t) mismatches += "T";

        if (!mismatches.empty())
        {
            diagnostics.push_back(ValidationDiagnostic{
                ValidationStage::mathematics,
                RejectionReason::modularMismatch,
                "P/Q/T component mismatch (" + mismatches
                    + ") modulo " + std::to_string(prime)
            });
        }
    }

    if (diagnostics.empty())
    {
        return ValidationResult::accepted();
    }

    return ValidationResult::rejected(std::move(diagnostics));
}

} // namespace pi::checkpoint
