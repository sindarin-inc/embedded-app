#include <catch_amalgamated.hpp>

#include "Sensors/TempleAngle.hpp"

extern "C" {
#include "Ulp/TempleLookup.h"
#include "Ulp/UlpTableLookup.h"
}

bool checkFunc(uint8_t x, uint8_t z) {
    int xx = (int)x - 128;
    int zz = (int)z - 128;
    return IsTempleOpen(xx, zz);
}

TEST_CASE("Test Temple Angle Lookup Table") {
    int errors = 0;
    for (int x = 0; x < 256; x++) {
        for (int z = 0; z < 256; z++) {
            bool t = checkTable(x, z);
            bool f = checkFunc(x, z);

            if (t != f) {
                errors++;
            }
        }
    }

    REQUIRE(errors == 0);
}
