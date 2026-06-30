#pragma once

#include "binary/BinaryNode.hpp"


namespace pi::binary
{

class BinarySplitter
{
public:

    [[nodiscard]]
    static BinaryNode split(
        std::size_t start,
        std::size_t end
    );

    [[nodiscard]]
    static BinaryNode splitSequential(
        std::size_t start,
        std::size_t end
    );

    [[nodiscard]]
    static BinaryNode splitParallel(
        std::size_t start,
        std::size_t end
    );

    [[nodiscard]]
    static BinaryNode merge(
        const BinaryNode& left,
        const BinaryNode& right
    );

};


} // namespace pi::binary