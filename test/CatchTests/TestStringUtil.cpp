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
