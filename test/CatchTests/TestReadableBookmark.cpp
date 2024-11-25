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
    SECTION("Test nested brackets and spaces") {
        MockFile file("+ [EPUB;offset:293,href:Brandon Sanderson - [Mistborn 01] - The Final "
                      "Empire_split_000.htm] [EPUB;offset:3146,href:Brandon Sanderson - [Mistborn "
                      "01] - The Final Empire_split_000.htm]\n");
        LocalBookmark bookmark;

        REQUIRE(ReadOneBookmark(file, bookmark) == true);
        REQUIRE(bookmark.status == BOOKMARK_ON);

        // Check first place
        REQUIRE(bookmark.firstPlace != nullptr);
        REQUIRE(bookmark.firstPlace->getDocType() == DocType::EPUB);
        REQUIRE(bookmark.firstPlace->getOffset() == 293);
        auto *firstEpubPlace = dynamic_cast<EPubPlace *>(bookmark.firstPlace.get());
        REQUIRE(firstEpubPlace != nullptr);
        REQUIRE(firstEpubPlace->getHRef() ==
                "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm");

        // Check last place
        REQUIRE(bookmark.lastPlace != nullptr);
        REQUIRE(bookmark.lastPlace->getDocType() == DocType::EPUB);
        REQUIRE(bookmark.lastPlace->getOffset() == 3146);
        auto *lastEpubPlace = dynamic_cast<EPubPlace *>(bookmark.lastPlace.get());
        REQUIRE(lastEpubPlace != nullptr);
        REQUIRE(lastEpubPlace->getHRef() ==
                "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm");
    }
}