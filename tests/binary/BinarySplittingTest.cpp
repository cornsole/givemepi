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


    BinaryNode splitResult =
        BinarySplitter::split(
            0,
            8
        );


    if (splitResult.start() != 0)
    {
        std::cerr
            << "Split start failed\n";

        return 1;
    }


    if (splitResult.end() != 8)
    {
        std::cerr
            << "Split end failed\n";

        return 1;
    }


    std::cout
        << "Recursive Split OK\n"; 
    

    
    BinaryNode sequentialResult =
        BinarySplitter::splitSequential(
            0,
            8
        );

    BinaryNode parallelResult =
        BinarySplitter::splitParallel(
            0,
            8
        );


    if (sequentialResult.start() != parallelResult.start())
    {
        std::cerr
            << "Parallel start mismatch\n";

        return 1;
    }


    if (sequentialResult.end() != parallelResult.end())
    {
        std::cerr
            << "Parallel end mismatch\n";

        return 1;
    }


    if (sequentialResult.P().toString() != parallelResult.P().toString())
    {
        std::cerr
            << "Parallel P mismatch\n";

        return 1;
    }


    if (sequentialResult.Q().toString() != parallelResult.Q().toString())
    {
        std::cerr
            << "Parallel Q mismatch\n";

        return 1;
    }


    if (sequentialResult.T().toString() != parallelResult.T().toString())
    {
        std::cerr
            << "Parallel T mismatch\n";

        return 1;
    }


    std::cout
        << "Parallel API OK\n";
    
    return 0;
}