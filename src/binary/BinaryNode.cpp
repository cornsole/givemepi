#include "binary/BinaryNode.hpp"


namespace pi::binary
{

BinaryNode::BinaryNode() = default;


BinaryNode::BinaryNode(
    std::size_t start,
    std::size_t end
)
    : start_(start),
      end_(end)
{
}


std::size_t BinaryNode::start() const noexcept
{
    return start_;
}


std::size_t BinaryNode::end() const noexcept
{
    return end_;
}


bigint::GMPInteger& BinaryNode::P() noexcept
{
    return p_;
}


bigint::GMPInteger& BinaryNode::Q() noexcept
{
    return q_;
}


bigint::GMPInteger& BinaryNode::T() noexcept
{
    return t_;
}


const bigint::GMPInteger& BinaryNode::P() const noexcept
{
    return p_;
}


const bigint::GMPInteger& BinaryNode::Q() const noexcept
{
    return q_;
}


const bigint::GMPInteger& BinaryNode::T() const noexcept
{
    return t_;
}

void BinaryNode::clearValues() noexcept
{
    p_ = bigint::GMPInteger{};
    q_ = bigint::GMPInteger{};
    t_ = bigint::GMPInteger{};
}

bool BinaryNode::hasValues() const noexcept
{
    return !p_.isZero() || !q_.isZero() || !t_.isZero();
}


} // namespace pi::binary
