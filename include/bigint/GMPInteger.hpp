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

    /**
     * Negates this integer in place.
     *
     * Input: the current arbitrary-precision integer value.
     * Output: the additive inverse of the current value.
     * Time complexity: O(n), where n is the number of limbs.
     * Memory complexity: O(1) auxiliary space.
     */
    void negate() noexcept;

    void set(
        const GMPInteger& other
    );


    [[nodiscard]]
    int compare(
        const GMPInteger& other
    ) const noexcept;


    void swap(
        GMPInteger& other
    ) noexcept;


    [[nodiscard]]
    bool isZero() const noexcept;


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
