#include "binary/BinarySplitter.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>


using pi::bigint::GMPInteger;

namespace pi::binary
{

namespace
{

static_assert(
    std::numeric_limits<std::size_t>::digits
    <= std::numeric_limits<std::uint64_t>::digits,
    "Binary Splitting term indices must fit in GMPInteger's input type"
);


BinaryNode makeChudnovskyLeaf(
    std::size_t term,
    std::size_t end
)
{
    BinaryNode node(
        term,
        end
    );


    if (term == 0)
    {
        node.P() = GMPInteger(1);
        node.Q() = GMPInteger(1);
        node.T() = GMPInteger(13591409);

        return node;
    }


    const GMPInteger k(
        static_cast<std::uint64_t>(term)
    );


    GMPInteger firstFactor(k);
    firstFactor.mul(GMPInteger(6));
    firstFactor.sub(GMPInteger(5));

    GMPInteger secondFactor(k);
    secondFactor.mul(GMPInteger(2));
    secondFactor.sub(GMPInteger(1));

    GMPInteger thirdFactor(k);
    thirdFactor.mul(GMPInteger(6));
    thirdFactor.sub(GMPInteger(1));


    node.P().set(firstFactor);
    node.P().mul(secondFactor);
    node.P().mul(thirdFactor);


    node.Q().set(k);
    node.Q().mul(k);
    node.Q().mul(k);
    node.Q().mul(GMPInteger(10939058860032000ULL));


    node.T().set(k);
    node.T().mul(GMPInteger(545140134));
    node.T().add(GMPInteger(13591409));
    node.T().mul(node.P());

    if (term % 2 != 0)
    {
        node.T().negate();
    }


    return node;
}

void validateRange(
    std::size_t start,
    std::size_t end
)
{
    if (start >= end)
    {
        throw std::invalid_argument(
            "Binary Splitting requires start < end"
        );
    }
}


BinaryNode splitSequentialValidated(
    std::size_t start,
    std::size_t end
)
{
    if (end - start == 1)
    {
        return makeChudnovskyLeaf(
            start,
            end
        );
    }


    const std::size_t mid =
        start + (end - start) / 2;


    BinaryNode left =
        splitSequentialValidated(
            start,
            mid
        );

    BinaryNode right =
        splitSequentialValidated(
            mid,
            end
        );


    return BinarySplitter::merge(
        left,
        right
    );
}

} // namespace

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
    validateRange(
        start,
        end
    );


    return splitSequentialValidated(
        start,
        end
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
