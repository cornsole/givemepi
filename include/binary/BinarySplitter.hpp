#pragma once

#include "binary/BinaryNode.hpp"


namespace pi::binary
{

class BinarySplitter
{
public:
    static BinaryNode split(
        std::size_t start,
        std::size_t end
    );

    static BinaryNode merge(
        const BinaryNode& left,
        const BinaryNode& right
    );

};


} // namespace pi::binary