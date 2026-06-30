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


} // namespace pi::binary