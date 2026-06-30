#include "binary/BinarySplitter.hpp"
using pi::bigint::GMPInteger;

namespace pi::binary
{

BinaryNode BinarySplitter::merge(
    const BinaryNode& left,
    const BinaryNode& right
)
{
    BinaryNode result(
        left.start(),
        right.end()
    );


    result.P().set(
        left.P()
    );

    result.P().mul(
        right.P()
    );


    result.Q().set(
        left.Q()
    );

    result.Q().mul(
        right.Q()
    );


    GMPInteger temp(
        left.T()
    );


    temp.mul(
        right.Q()
    );


    result.T().set(
        left.P()
    );

    result.T().mul(
        right.T()
    );


    temp.add(
        result.T()
    );


    result.T().swap(
        temp
    );


    return result;
}

BinaryNode BinarySplitter::split(
    std::size_t start,
    std::size_t end
)
{
    return splitSequential(
        start,
        end
    );
}

BinaryNode BinarySplitter::splitSequential(
    std::size_t start,
    std::size_t end
)
{
    if (end - start == 1)
    {
        BinaryNode node(
            start,
            end
        );

        // leaf 계산 예정

        return node;
    }


    std::size_t mid =
        start + (end - start) / 2;


    BinaryNode left =
        splitSequential(
            start,
            mid
        );

    BinaryNode right =
        splitSequential(
            mid,
            end
        );


    return merge(
        left,
        right
    );
}

BinaryNode BinarySplitter::splitParallel(
    std::size_t start,
    std::size_t end
)
{
    // Placeholder.
    // Scheduler-based parallel execution will be introduced
    // after task synchronization support is available.

    return splitSequential(
        start,
        end
    );
}

} // namespace pi::binary