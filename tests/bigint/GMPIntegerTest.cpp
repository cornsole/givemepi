#include "bigint/GMPInteger.hpp"

#include <iostream>


using pi::bigint::GMPInteger;


int main()
{
    GMPInteger a(
        100
    );


    GMPInteger b(
        50
    );


    a.add(
        b
    );


    if (a.toString() != "150")
    {
        std::cerr
            << "Add failed\n";

        return 1;
    }


    a.sub(
        b
    );


    if (a.toString() != "100")
    {
        std::cerr
            << "Sub failed\n";

        return 1;
    }


    a.mul(
        b
    );


    if (a.toString() != "5000")
    {
        std::cerr
            << "Mul failed\n";

        return 1;
    }


    GMPInteger copy(
        a
    );


    if (copy.toString() != "5000")
    {
        std::cerr
            << "Copy failed\n";

        return 1;
    }


    GMPInteger moved(
        std::move(copy)
    );


    if (moved.toString() != "5000")
    {
        std::cerr
            << "Move failed\n";

        return 1;
    }


    std::cout
        << "GMPInteger OK\n";


    std::cout
        << "Value: "
        << moved.toString()
        << '\n';


    return 0;
}