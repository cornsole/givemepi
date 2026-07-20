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
     * Replaces this integer with 10 raised to exponent.
     *
     * Time complexity: O(M(n) log(exponent)) for an n-bit result.
     * Memory complexity: O(n).
     *
     * @throws std::overflow_error if exponent is unsupported by GMP.
     */
    void setPowerOfTen(
        std::uint64_t exponent
    );

    /**
     * Replaces a non-negative value with its floor integer square root.
     *
     * Time complexity: O(M(n)) for an n-bit input.
     * Memory complexity: O(n).
     *
     * @throws std::domain_error if this integer is negative.
     */
    void floorSquareRoot();

    /**
     * Replaces this integer with floor(this / divisor).
     *
     * Time complexity: O(M(n)) for n-bit operands.
     * Memory complexity: O(n).
     *
     * @throws std::domain_error if divisor is zero.
     */
    void floorDivide(
        const GMPInteger& divisor
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
