#pragma once

#include "bigint/GMPInteger.hpp"

#include <cstddef>


namespace pi::binary
{

class BinaryNode
{
public:

    BinaryNode();


    BinaryNode(
        std::size_t start,
        std::size_t end
    );


    [[nodiscard]]
    std::size_t start() const noexcept;


    [[nodiscard]]
    std::size_t end() const noexcept;


    [[nodiscard]]
    bigint::GMPInteger& P() noexcept;


    [[nodiscard]]
    bigint::GMPInteger& Q() noexcept;


    [[nodiscard]]
    bigint::GMPInteger& T() noexcept;


    [[nodiscard]]
    const bigint::GMPInteger& P() const noexcept;


    [[nodiscard]]
    const bigint::GMPInteger& Q() const noexcept;


    [[nodiscard]]
    const bigint::GMPInteger& T() const noexcept;


private:

    std::size_t start_ = 0;

    std::size_t end_ = 0;


    bigint::GMPInteger p_;

    bigint::GMPInteger q_;

    bigint::GMPInteger t_;

};


} // namespace pi::binary