#pragma once

#include "binary/BinaryNode.hpp"


namespace pi::binary
{

class BinarySplitter
{
public:

    /**
     * Computes the Binary Splitting node for the half-open range [start, end).
     *
     * @throws std::invalid_argument if start is greater than or equal to end.
     */
    [[nodiscard]]
    static BinaryNode split(
        std::size_t start,
        std::size_t end
    );

    /**
     * Computes [start, end) using recursive sequential Binary Splitting.
     *
     * @throws std::invalid_argument if start is greater than or equal to end.
     */
    [[nodiscard]]
    static BinaryNode splitSequential(
        std::size_t start,
        std::size_t end
    );

    /**
     * Computes [start, end) through the parallel-compatible entry point.
     *
     * @throws std::invalid_argument if start is greater than or equal to end.
     */
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
