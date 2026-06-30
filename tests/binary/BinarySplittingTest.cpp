#include "binary/BinaryNode.hpp"
#include "binary/BinarySplitter.hpp"

#include <iostream>


using pi::binary::BinaryNode;
using pi::binary::BinarySplitter;


int main()
{
    BinaryNode left(
        0,
        5
    );


    BinaryNode right(
        5,
        10
    );


    left.P() = pi::bigint::GMPInteger(
        2
    );

    left.Q() = pi::bigint::GMPInteger(
        3
    );

    left.T() = pi::bigint::GMPInteger(
        4
    );


    right.P() = pi::bigint::GMPInteger(
        5
    );

    right.Q() = pi::bigint::GMPInteger(
        7
    );

    right.T() = pi::bigint::GMPInteger(
        11
    );


    BinaryNode result =
        BinarySplitter::merge(
            left,
            right
        );


    // P = 2 * 5
    if (result.P().toString() != "10")
    {
        std::cerr
            << "P merge failed\n";

        return 1;
    }


    // Q = 3 * 7
    if (result.Q().toString() != "21")
    {
        std::cerr
            << "Q merge failed\n";

        return 1;
    }


    // T = 4 * 7 + 2 * 11
    //   = 50
    if (result.T().toString() != "50")
    {
        std::cerr
            << "T merge failed\n";

        return 1;
    }


    std::cout
        << "BinarySplitting OK\n";


    std::cout
        << "P="
        << result.P().toString()
        << " Q="
        << result.Q().toString()
        << " T="
        << result.T().toString()
        << '\n';


    return 0;
}