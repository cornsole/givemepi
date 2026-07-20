#include "bigint/GMPInteger.hpp"

#include <limits>
#include <stdexcept>
#include <utility>


namespace pi::bigint
{

GMPInteger::GMPInteger()
{
    mpz_init(
        value_
    );
}


GMPInteger::GMPInteger(
    std::uint64_t value
)
{
    mpz_init_set_ui(
        value_,
        value
    );
}


GMPInteger::~GMPInteger()
{
    mpz_clear(
        value_
    );
}


GMPInteger::GMPInteger(
    const GMPInteger& other
)
{
    mpz_init_set(
        value_,
        *other.raw()
    );
}


GMPInteger::GMPInteger(
    GMPInteger&& other
) noexcept
{
    mpz_init(
        value_
    );

    mpz_swap(
        value_,
        *other.raw()
    );
}


GMPInteger& GMPInteger::operator=(
    const GMPInteger& other
)
{
    if (this != &other)
    {
        mpz_set(
            value_,
            *other.raw()
        );
    }

    return *this;
}


GMPInteger& GMPInteger::operator=(
    GMPInteger&& other
) noexcept
{
    if (this != &other)
    {
        mpz_swap(
            value_,
            *other.raw()
        );
    }

    return *this;
}


void GMPInteger::add(
    const GMPInteger& other
)
{
    mpz_add(
        value_,
        value_,
        *other.raw()
    );
}


void GMPInteger::sub(
    const GMPInteger& other
)
{
    mpz_sub(
        value_,
        value_,
        *other.raw()
    );
}


void GMPInteger::mul(
    const GMPInteger& other
)
{
    mpz_mul(
        value_,
        value_,
        *other.raw()
    );
}


void GMPInteger::setPowerOfTen(
    std::uint64_t exponent
)
{
    if (exponent > std::numeric_limits<unsigned long>::max())
    {
        throw std::overflow_error(
            "Power-of-ten exponent is unsupported by GMP"
        );
    }

    mpz_ui_pow_ui(
        value_,
        10,
        static_cast<unsigned long>(exponent)
    );
}


void GMPInteger::floorSquareRoot()
{
    if (mpz_sgn(value_) < 0)
    {
        throw std::domain_error(
            "Integer square root requires a non-negative value"
        );
    }

    mpz_sqrt(
        value_,
        value_
    );
}


void GMPInteger::floorDivide(
    const GMPInteger& divisor
)
{
    if (divisor.isZero())
    {
        throw std::domain_error(
            "Integer division by zero"
        );
    }

    mpz_fdiv_q(
        value_,
        value_,
        *divisor.raw()
    );
}


void GMPInteger::negate() noexcept
{
    mpz_neg(
        value_,
        value_
    );
}


void GMPInteger::set(
    const GMPInteger& other
)
{
    mpz_set(
        value_,
        *other.raw()
    );
}


int GMPInteger::compare(
    const GMPInteger& other
) const noexcept
{
    return mpz_cmp(
        value_,
        *other.raw()
    );
}


void GMPInteger::swap(
    GMPInteger& other
) noexcept
{
    mpz_swap(
        value_,
        *other.raw()
    );
}


bool GMPInteger::isZero() const noexcept
{
    return mpz_sgn(
        value_
    ) == 0;
}


std::string GMPInteger::toString() const
{
    char* result =
        mpz_get_str(
            nullptr,
            10,
            value_
        );


    std::string output(
        result
    );


    void (*freeFunc)(void*, size_t);

    mp_get_memory_functions(
        nullptr,
        nullptr,
        &freeFunc
    );


    freeFunc(
        result,
        output.size() + 1
    );


    return output;
}


mpz_t* GMPInteger::raw() noexcept
{
    return &value_;
}


const mpz_t* GMPInteger::raw() const noexcept
{
    return &value_;
}


} // namespace pi::bigint
