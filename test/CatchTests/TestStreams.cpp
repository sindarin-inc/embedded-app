#include <catch_amalgamated.hpp>

#include "Storage/CustomIstream.hpp"
#include "Storage/IStreamFileImpl.hpp"

TEST_CASE("Test IStreamFileImpl") {
    // Create an istream
    std::stringstream stream("This is a test string omg is it going to work?");

    // Create an IStreamFileImpl
    auto file = File(std::make_shared<IstreamFileImpl>(stream));

    uint8_t buffer[100];
    file.read(buffer, 10);
    REQUIRE(std::string((char *)buffer, 10) == "This is a ");

    file.read(buffer, 12);
    REQUIRE(std::string((char *)buffer, 12) == "test string ");

    file.seek(30);
    file.read(buffer, 12);
    REQUIRE(std::string((char *)buffer, 12) == "t going to w");

    REQUIRE(file.position() == 42);

    // seek backwards
    file.seek(5);
    file.read(buffer, 10);
    REQUIRE(std::string((char *)buffer, 10) == "is a test ");

    REQUIRE(file.position() == 15);
}
