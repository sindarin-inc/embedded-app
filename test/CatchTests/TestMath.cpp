#include <catch_amalgamated.hpp>

#include "Misc/Math.hpp"

TEST_CASE("Test MapRange") {
    REQUIRE(MapRange(0, 0, 100, 0, 100) == 0);
    // Support for floats
    REQUIRE(MapRange(0.1, 0.0, 10.0, 3.0, 203.0) == 5);
    // Support for flipped ranges
    REQUIRE(MapRange(5, 0, 25, 100, 50) == 90);
    REQUIRE(MapRange(5, 0, 25, -100, -50) == -90);
}
