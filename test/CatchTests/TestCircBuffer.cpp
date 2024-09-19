#include <catch_amalgamated.hpp>

#include "Misc/CircBuffer.hpp"

TEST_CASE("CircBuffer push and size", "[CircBuffer]") {
    CircBuffer buffer(10);

    SECTION("Push within capacity") {
        uint8_t data1[] = {1, 2, 3, 4, 5};
        buffer.push(data1, 5);
        REQUIRE(buffer.size() == 5);

        uint8_t data2[] = {6, 7, 8};
        buffer.push(data2, 3);
        REQUIRE(buffer.size() == 8);
    }

    SECTION("Push beyond capacity") {
        uint8_t data1[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        buffer.push(data1, 12);
        REQUIRE(buffer.size() == 10); // Capacity should limit the size

        uint8_t expected[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        uint8_t output[10];
        buffer.copyFrom(0, output, 10);
        REQUIRE(memcmp(output, expected, 10) == 0);
    }
}

TEST_CASE("CircBuffer copyFrom", "[CircBuffer]") {
    CircBuffer buffer(10);

    SECTION("Copy within buffer") {
        uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        buffer.push(data, 10);

        uint8_t output[5];
        buffer.copyFrom(2, output, 5);

        uint8_t expected[] = {3, 4, 5, 6, 7};
        REQUIRE(memcmp(output, expected, 5) == 0);
    }

    SECTION("Copy with wrap around") {
        uint8_t data1[] = {1, 2, 3, 4, 5};
        uint8_t data2[] = {6, 7, 8, 9, 10};
        buffer.push(data1, 5);
        buffer.push(data2, 5);

        uint8_t output[5];
        buffer.copyFrom(8, output, 2);

        uint8_t expected[] = {9, 10};
        REQUIRE(memcmp(output, expected, 2) == 0);
    }
}

TEST_CASE("CircBuffer moveFrom", "[CircBuffer]") {
    CircBuffer buffer(10);

    SECTION("Move within buffer") {
        uint8_t data[] = {1, 2, 3, 4, 5};
        buffer.push(data, 5);

        uint8_t output[5];
        buffer.moveFrom(0, output, 5);

        uint8_t expected[] = {1, 2, 3, 4, 5};
        REQUIRE(memcmp(output, expected, 5) == 0);
        REQUIRE(buffer.size() == 0);
    }

    SECTION("Move with wrap around") {
        uint8_t data1[] = {1, 2, 3, 4, 5};
        uint8_t data2[] = {6, 7, 8, 9, 10};
        buffer.push(data1, 5);
        buffer.push(data2, 5);

        uint8_t output[3];
        buffer.moveFrom(7, output, 3);

        uint8_t expected[] = {8, 9, 10};
        REQUIRE(memcmp(output, expected, 3) == 0);
        REQUIRE(buffer.size() == 7);
    }
}

TEST_CASE("CircBuffer clear", "[CircBuffer]") {
    CircBuffer buffer(10);

    SECTION("Clear buffer") {
        uint8_t data[] = {1, 2, 3, 4, 5};
        buffer.push(data, 5);
        REQUIRE(buffer.size() == 5);

        buffer.clear();
        REQUIRE(buffer.size() == 0);
    }
}
