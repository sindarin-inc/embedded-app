#include <catch_amalgamated.hpp>

#include "Misc/StringUtil.hpp"

TEST_CASE("Test HasSuffix") {
    REQUIRE(HasSuffix("this/is-thefile.txt.gz", ".gz"));
    REQUIRE(!HasSuffix("this/is-thefile.txt.gz", ".epub"));
    REQUIRE(!HasSuffix("", ".gz"));
    REQUIRE(HasSuffix(".gz", ".gz"));
    REQUIRE(HasSuffix("somefilename.epub", ".epub"));
    REQUIRE(HasSuffix("somefilename.epub", ""));
}

TEST_CASE("Test Commify") {
    REQUIRE(Commify(0) == "0");
    REQUIRE(Commify(1) == "1");
    REQUIRE(Commify(10) == "10");
    REQUIRE(Commify(100) == "100");
    REQUIRE(Commify(1000) == "1,000");
    REQUIRE(Commify(10000) == "10,000");
    REQUIRE(Commify(100000) == "100,000");
    REQUIRE(Commify(1000000) == "1,000,000");
    REQUIRE(Commify(10000000) == "10,000,000");
    REQUIRE(Commify(100000000) == "100,000,000");
    REQUIRE(Commify(1000000000) == "1,000,000,000");
}
