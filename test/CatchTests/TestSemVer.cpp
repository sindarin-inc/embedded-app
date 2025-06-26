#include <catch_amalgamated.hpp>

#include "Misc/SemVer.hpp"

TEST_CASE("Test SemVer", "[semver]") {
    REQUIRE(SemVerCompareVersions("1.0.0", "1.0.0") == SemVerComparison::EqualTo);
    REQUIRE(SemVerCompareVersions("1.0.0", "1.0.1") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.0.1", "1.0.0") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.0.0", "1.1.0") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.1.0", "1.0.0") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.0.0", "2.0.0") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("2.0.0", "1.0.0") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.0.1", "1.0.0.99") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.0.0.99", "1.0.1") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.0.99", "1.1.0") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.1.0", "1.0.99") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.0.0", "") == SemVerComparison::Invalid);
    REQUIRE(SemVerCompareVersions("", "1.0.0") == SemVerComparison::Invalid);
    REQUIRE(SemVerCompareVersions("asdf", "1.0.0") == SemVerComparison::Invalid);
}

TEST_CASE("Test SemVer with complex version numbers", "[semver]") {
    REQUIRE(SemVerCompareVersions("1.5.0", "1.5.0.99") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.5.0.99", "1.5.0") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.5.0.99", "1.5.0.99.1") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.5.0.99.1", "1.5.0.99") == SemVerComparison::GreaterThan);
    REQUIRE(SemVerCompareVersions("1.5.0.99.1", "1.5.1") == SemVerComparison::LessThan);
    REQUIRE(SemVerCompareVersions("1.5.1", "1.5.0.99.1") == SemVerComparison::GreaterThan);
}

TEST_CASE("Test SemVer with v prefix", "[semver]") {
    REQUIRE(SemVerCompareVersions("v1.5.0", "v1.5.0.99") == SemVerComparison::Invalid);
    REQUIRE(SemVerCompareVersions("v1.5.0.99.1", "v1.5.1") == SemVerComparison::Invalid);
}
