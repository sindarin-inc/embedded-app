#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Models/ReadableBookmark.hpp"
#undef protected
#undef private

#include <memory>
#include <sstream>

#include "Storage/SFile.hpp"

class MockFile : public SFile {
    std::stringstream stream;

public:
    explicit MockFile(const std::string &content) : stream(content) {}

    auto available() -> int override { return !stream.eof(); }
    auto read() -> int override { return stream.get(); }
    auto readStringUntil(char terminator) -> std::string override {
        std::string result;
        std::getline(stream, result, terminator);
        return result;
    }
};

TEST_CASE("ReadOneBookmark functionality", "[readable-bookmark]") {
    SECTION("Read valid ON bookmark") {
        MockFile file(
            "+ [EPUB;offset:100,href:chapter1.htm] [EPUB;offset:200,href:chapter1.htm]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(*bookmark.firstPlace == EPubPlace(100, "chapter1.htm"));
        REQUIRE(*bookmark.lastPlace == EPubPlace(200, "chapter1.htm"));
    }

    SECTION("Read valid OFF bookmark") {
        MockFile file(
            "- [EPUB;offset:100,href:chapter1.htm] [EPUB;offset:200,href:chapter1.htm]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_OFF);
        REQUIRE(*bookmark.firstPlace == EPubPlace(100, "chapter1.htm"));
        REQUIRE(*bookmark.lastPlace == EPubPlace(200, "chapter1.htm"));
    }

    SECTION("Read TXT bookmark") {
        MockFile file("+ [TXT;offset:100] [TXT;offset:200]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(*bookmark.firstPlace == TxtPlace(100));
        REQUIRE(*bookmark.lastPlace == TxtPlace(200));
    }

    SECTION("Invalid bookmark format") {
        MockFile file("x [invalid] [format]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == false);
    }

    SECTION("Missing space after status character") {
        MockFile file("+[invalid][format]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == false);
    }

    SECTION("Empty file") {
        MockFile file("");
        LocalBookmark bookmark;

        REQUIRE(ReadOneBookmark(file, bookmark) == false);
    }

    SECTION("Test nested brackets and spaces") {
        MockFile file("+ [EPUB;offset:293,href:Brandon Sanderson - [Mistborn 01] - The Final "
                      "Empire_split_000.htm] [EPUB;offset:3146,href:Brandon Sanderson - [Mistborn "
                      "01] - The Final Empire_split_000.htm]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(
            *bookmark.firstPlace ==
            EPubPlace(293, "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm"));
        REQUIRE(
            *bookmark.lastPlace ==
            EPubPlace(3146, "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm"));
    }
}