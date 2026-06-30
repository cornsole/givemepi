#pragma once

#include <gmp.h>

#include <cstdint>
#include <string>


namespace pi::bigint
{

class GMPInteger
{
public:

    GMPInteger();

    explicit GMPInteger(
        std::uint64_t value
    );


    ~GMPInteger();


    GMPInteger(
        const GMPInteger& other
    );


    GMPInteger(
        GMPInteger&& other
    ) noexcept;


    GMPInteger& operator=(
        const GMPInteger& other
    );


    GMPInteger& operator=(
        GMPInteger&& other
    ) noexcept;


    void add(
        const GMPInteger& other
    );


    void sub(
        const GMPInteger& other
    );


    void mul(
        const GMPInteger& other
    );


    [[nodiscard]]
    std::string toString() const;


    [[nodiscard]]
    mpz_t* raw() noexcept;


    [[nodiscard]]
    const mpz_t* raw() const noexcept;


private:

    mpz_t value_;

};


} // namespace pi::bigint