#include <catch_amalgamated.hpp>
#include <string>
#include <vector>

#include "UI/Fonts/IBMFDriver/IBMFDefs.hpp"
#include "UI/Fonts/IBMFDriver/UTF8Iterator.hpp"

#define GOOD_1 "\x24"
#define GOOD_2 "\xD0\x98"
#define GOOD_3 "\xE2\x82\xAC"
#define GOOD_4 "\xF0\x90\x8D\x88"

#define CODE_1 (char32_t)(0x24)
#define CODE_2 (char32_t)(0x0418)
#define CODE_3 (char32_t)(0x20AC)
#define CODE_4 (char32_t)(0x10348)

#define BAD_1 "\xD0"
#define BAD_2 "\xE2\x82"
#define BAD_3 "\xF0\x8D\x88"
#define BAD_4 "\x90\x8D\x88"
#define BAD_5 "\xF0\x90\x8D"
#define BAD_6 "\xF0\x90"

#define BAD_CODE ibmf_defs::UNKNOWN_CODEPOINT

const std::string Line1{GOOD_1 GOOD_2 GOOD_3 GOOD_4};
const std::vector Line1Result{CODE_1, CODE_2, CODE_3, CODE_4};

const std::string Line2{GOOD_3 GOOD_2 GOOD_1 GOOD_4};
const std::vector Line2Result{CODE_3, CODE_2, CODE_1, CODE_4};

const std::string Line3{GOOD_1 BAD_4 BAD_2 GOOD_4};
const std::vector Line3Result{CODE_1, BAD_CODE, BAD_CODE, CODE_4};

const std::string Line4{BAD_1 GOOD_1 BAD_1 GOOD_2 BAD_1};
const std::string Line5{BAD_2 GOOD_1 BAD_2 GOOD_2 BAD_2};
const std::string Line6{BAD_3 GOOD_1 BAD_3 GOOD_2 BAD_3};
const std::string Line7{BAD_4 GOOD_1 BAD_4 GOOD_2 BAD_4};
const std::string Line8{BAD_5 GOOD_1 BAD_5 GOOD_2 BAD_5};
const std::string Line9{BAD_6 GOOD_1 BAD_6 GOOD_2 BAD_6};
const std::vector LineXResult{BAD_CODE, CODE_1, BAD_CODE, CODE_2, BAD_CODE};

const std::string Line10{BAD_1 GOOD_3 BAD_1 GOOD_4 BAD_1};
const std::string Line11{BAD_2 GOOD_3 BAD_2 GOOD_4 BAD_2};
const std::string Line12{BAD_3 GOOD_3 BAD_3 GOOD_4 BAD_3};
const std::string Line13{BAD_4 GOOD_3 BAD_4 GOOD_4 BAD_4};
const std::string Line14{BAD_5 GOOD_3 BAD_5 GOOD_4 BAD_5};
const std::string Line15{BAD_6 GOOD_3 BAD_6 GOOD_4 BAD_6};
const std::vector LineYResult{BAD_CODE, CODE_3, BAD_CODE, CODE_4, BAD_CODE};

auto CheckUTF8Translation(const std::string &str, const std::vector<char32_t> &result) -> bool {

    auto it = UTF8Iterator(str);
    int i = 0;

    while (it != str.end()) {
        char32_t ch = *it++;
        if (result[i] != ch) {
            UNSCOPED_INFO("UTF8 translation at Index " << i << " Is bad! Expected: " << +result[i]
                                                       << " Got: " << +ch);
            return false;
        }
        i++;
    }

    return true;
}

TEST_CASE("UTF8 Iterator") {
    SECTION("Line 1") { CHECK(CheckUTF8Translation(Line1, Line1Result)); }
    SECTION("Line 2") { CHECK(CheckUTF8Translation(Line2, Line2Result)); }
    SECTION("Line 3") { CHECK(CheckUTF8Translation(Line3, Line3Result)); }
    SECTION("Line 4") { CHECK(CheckUTF8Translation(Line4, LineXResult)); }
    SECTION("Line 5") { CHECK(CheckUTF8Translation(Line5, LineXResult)); }
    SECTION("Line 6") { CHECK(CheckUTF8Translation(Line6, LineXResult)); }
    SECTION("Line 7") { CHECK(CheckUTF8Translation(Line7, LineXResult)); }
    SECTION("Line 8") { CHECK(CheckUTF8Translation(Line8, LineXResult)); }
    SECTION("Line 9") { CHECK(CheckUTF8Translation(Line9, LineXResult)); }
    SECTION("Line 10") { CHECK(CheckUTF8Translation(Line10, LineYResult)); }
    SECTION("Line 11") { CHECK(CheckUTF8Translation(Line11, LineYResult)); }
    SECTION("Line 12") { CHECK(CheckUTF8Translation(Line12, LineYResult)); }
    SECTION("Line 13") { CHECK(CheckUTF8Translation(Line13, LineYResult)); }
    SECTION("Line 14") { CHECK(CheckUTF8Translation(Line14, LineYResult)); }
    SECTION("Line 15") { CHECK(CheckUTF8Translation(Line15, LineYResult)); }
}
