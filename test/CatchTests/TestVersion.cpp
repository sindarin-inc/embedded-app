#include <catch_amalgamated.hpp>

#include "Version.hpp"

TEST_CASE("Test Version", "[.][version]") {
    REQUIRE(ExtractVersion("release/v0.7.1") == "0.7.1");
    REQUIRE(ExtractVersion("release/v0.7.1-dirty") == "0.7.1");
    REQUIRE(ExtractVersion("release/v0.7.1-2-ad873cbd") == "0.7.1");
    REQUIRE(ExtractVersion("release/v0.7.1-2-ad873cbd-dir") == "0.7.1");
}
