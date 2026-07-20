#include "bigint/GMPInteger.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>


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
    
    GMPInteger setTarget(
        0
    );


    setTarget.set(
        a
    );


    if (setTarget.toString() != "5000")
    {
        std::cerr
            << "Set failed\n";

        return 1;
    }


    if (setTarget.compare(a) != 0)
    {
        std::cerr
            << "Compare equal failed\n";

        return 1;
    }


    GMPInteger bigger(
        9000
    );


    if (bigger.compare(setTarget) <= 0)
    {
        std::cerr
            << "Compare greater failed\n";

        return 1;
    }


    GMPInteger zero;


    if (!zero.isZero())
    {
        std::cerr
            << "Zero check failed\n";

        return 1;
    }


    zero.swap(
        setTarget
    );


    if (zero.toString() != "5000")
    {
        std::cerr
            << "Swap failed\n";

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


    GMPInteger positive(
        42
    );


    positive.negate();


    if (positive.toString() != "-42")
    {
        std::cerr
            << "Positive negate failed\n";

        return 1;
    }


    positive.negate();


    if (positive.toString() != "42")
    {
        std::cerr
            << "Negative negate failed\n";

        return 1;
    }


    GMPInteger signedZero;


    signedZero.negate();


    if (!signedZero.isZero() || signedZero.toString() != "0")
    {
        std::cerr
            << "Zero negate failed\n";

        return 1;
    }


    GMPInteger powerOfTen;
    powerOfTen.setPowerOfTen(0);

    if (powerOfTen.toString() != "1")
    {
        std::cerr
            << "Power-of-ten zero exponent failed\n";

        return 1;
    }

    powerOfTen.setPowerOfTen(20);

    if (powerOfTen.toString() != "100000000000000000000")
    {
        std::cerr
            << "Power-of-ten calculation failed\n";

        return 1;
    }


    GMPInteger perfectSquare(144);
    perfectSquare.floorSquareRoot();

    GMPInteger nonPerfectSquare(200);
    nonPerfectSquare.floorSquareRoot();

    GMPInteger zeroSquareRoot;
    zeroSquareRoot.floorSquareRoot();

    if (perfectSquare.toString() != "12"
        || nonPerfectSquare.toString() != "14"
        || !zeroSquareRoot.isZero())
    {
        std::cerr
            << "Floor square root failed\n";

        return 1;
    }


    GMPInteger negativeSquareRoot(4);
    negativeSquareRoot.negate();
    bool rejectedNegativeSquareRoot = false;

    try
    {
        negativeSquareRoot.floorSquareRoot();
    }
    catch (const std::domain_error&)
    {
        rejectedNegativeSquareRoot = true;
    }

    if (!rejectedNegativeSquareRoot
        || negativeSquareRoot.toString() != "-4")
    {
        std::cerr
            << "Negative square root contract failed\n";

        return 1;
    }


    const GMPInteger divisor(4);
    GMPInteger exactDividend(100);
    exactDividend.floorDivide(divisor);

    GMPInteger truncatedDividend(101);
    truncatedDividend.floorDivide(divisor);

    GMPInteger negativeDividend(101);
    negativeDividend.negate();
    negativeDividend.floorDivide(divisor);

    if (exactDividend.toString() != "25"
        || truncatedDividend.toString() != "25"
        || negativeDividend.toString() != "-26")
    {
        std::cerr
            << "Floor division failed\n";

        return 1;
    }


    GMPInteger zeroDivisorDividend(101);
    bool rejectedZeroDivisor = false;

    try
    {
        zeroDivisorDividend.floorDivide(GMPInteger(0));
    }
    catch (const std::domain_error&)
    {
        rejectedZeroDivisor = true;
    }

    if (!rejectedZeroDivisor
        || zeroDivisorDividend.toString() != "101")
    {
        std::cerr
            << "Zero divisor contract failed\n";

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
