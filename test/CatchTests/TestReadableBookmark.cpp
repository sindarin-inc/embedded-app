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
        REQUIRE(bookmark.firstPlace->str() == EPubPlace(100, "chapter1.htm").str());
        REQUIRE(bookmark.lastPlace->str() == EPubPlace(200, "chapter1.htm").str());
    }

    SECTION("Read valid OFF bookmark") {
        MockFile file(
            "- [EPUB;offset:100,href:chapter1.htm] [EPUB;offset:200,href:chapter1.htm]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_OFF);
        REQUIRE(bookmark.firstPlace->str() == EPubPlace(100, "chapter1.htm").str());
        REQUIRE(bookmark.lastPlace->str() == EPubPlace(200, "chapter1.htm").str());
    }

    SECTION("Read TXT bookmark") {
        MockFile file("+ [TXT;offset:100] [TXT;offset:200]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(bookmark.firstPlace->str() == TxtPlace(100).str());
        REQUIRE(bookmark.lastPlace->str() == TxtPlace(200).str());
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
        REQUIRE(bookmark.firstPlace->str() ==
                EPubPlace(293, "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm")
                    .str());
        REQUIRE(
            bookmark.lastPlace->str() ==
            EPubPlace(3146, "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm")
                .str());
    }

    SECTION("Unpaired brackets in first place") {
        MockFile file(
            "+ [EPUB;offset:100,href:Bad[File.epub] [EPUB;offset:200,href:chapter2.epub]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark));
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(bookmark.firstPlace->str() == EPubPlace(100, "Bad[File.epub").str());
        REQUIRE(bookmark.lastPlace->str() == EPubPlace(200, "chapter2.epub").str());
    }

    SECTION("Unpaired brackets in second place") {
        MockFile file(
            "+ [EPUB;offset:100,href:chapter1.epub] [EPUB;offset:200,href:Bad[File.epub]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark));
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(bookmark.firstPlace->str() == EPubPlace(100, "chapter1.epub").str());
        REQUIRE(bookmark.lastPlace->str() == EPubPlace(200, "Bad[File.epub").str());
    }

    SECTION("Multiple unpaired brackets") {
        MockFile file(
            "+ [EPUB;offset:100,href:Bad[[File.epub] [EPUB;offset:200,href:Also]]Bad.epub]\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark));
        REQUIRE(bookmark.status == BOOKMARK_ON);
        REQUIRE(bookmark.firstPlace->str() == EPubPlace(100, "Bad[[File.epub").str());
        REQUIRE(bookmark.lastPlace->str() == EPubPlace(200, "Also]]Bad.epub").str());
    }

    SECTION("Missing final bracket") {
        MockFile file(
            "+ [EPUB;offset:100,href:chapter1.epub] [EPUB;offset:200,href:chapter2.epub\n");
        LocalBookmark bookmark;
        REQUIRE(ReadOneBookmark(file, bookmark) == false);
        REQUIRE(bookmark.firstPlace->str() == EPubPlace(100, "chapter1.epub").str());
        REQUIRE(bookmark.lastPlace == nullptr);
    }
}