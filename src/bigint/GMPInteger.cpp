#include "bigint/GMPInteger.hpp"

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