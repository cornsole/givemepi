#include "verification/SHA256.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>


namespace
{

template<typename Exception, typename Function>
bool throws(Function&& function)
{
    try
    {
        function();
    }
    catch (const Exception&)
    {
        return true;
    }
    return false;
}

} // namespace


int main()
{
    using pi::verification::SHA256;
    using pi::verification::toHex;

    assert(toHex(SHA256::hash(""))
        == "e3b0c44298fc1c149afbf4c8996fb924"
           "27ae41e4649b934ca495991b7852b855");
    assert(toHex(SHA256::hash("abc"))
        == "ba7816bf8f01cfea414140de5dae2223"
           "b00361a396177a9cb410ff61f20015ad");
    assert(toHex(SHA256::hash(
        "abcdbcdecdefdefgefghfghighijhijk"
        "ijkljklmklmnlmnomnopnopq"))
        == "248d6a61d20638b8e5c026930c3e6039"
           "a33ce45964ff2167f6ecedd419db06c1");

    SHA256 chunked;
    const std::string message(1'000'000, 'a');
    for (std::size_t offset = 0; offset < message.size(); offset += 137)
    {
        chunked.update(std::string_view(message).substr(
            offset,
            std::min<std::size_t>(137, message.size() - offset)
        ));
    }
    const auto millionDigest = chunked.finalize();
    assert(toHex(millionDigest)
        == "cdc76e5c9914fb9281a1c7e284d73e67"
           "f1809a48a497200e046d39ccc7112cd0");
    assert(chunked.finalized());
    assert(chunked.finalize() == millionDigest);
    assert(throws<std::logic_error>([&chunked]() {
        chunked.update("later");
    }));

    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / ("givemepi-sha256-" + std::to_string(::getpid()));
    {
        std::ofstream output(path, std::ios::binary);
        output << "3.14159\nignored";
    }
    assert(SHA256::hashFilePrefix(path, 7) == SHA256::hash("3.14159"));
    assert(throws<std::runtime_error>([&path]() {
        static_cast<void>(SHA256::hashFilePrefix(path, 100));
    }));
    std::filesystem::remove(path);

    return 0;
}
