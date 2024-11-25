#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Models/Place.hpp"
#undef protected
#undef private

TEST_CASE("Test Place factory", "[place]") {
    SECTION("Test TXT place parsing") {
        // Test explicit TXT format
        auto place1 = Place::factory("[TXT;offset:123]");
        REQUIRE(place1->isFor(DocType::TXT));
        REQUIRE(place1->getOffset() == 123);

        // Test simple number format (defaults to TXT)
        auto place2 = Place::factory("456");
        REQUIRE(place2->isFor(DocType::TXT));
        REQUIRE(place2->getOffset() == 456);

        // Test invalid TXT format returns offset 0
        auto place3 = Place::factory("[TXT;offset:invalid]");
        REQUIRE(place3->isFor(DocType::TXT));
        REQUIRE(place3->getOffset() == 0);
    }

    SECTION("Test EPUB place parsing") {
        // Test valid EPUB format
        auto place1 = Place::factory("[EPUB;offset:789,href:chapter1.html]");
        REQUIRE(place1->isFor(DocType::EPUB));
        REQUIRE(place1->getOffset() == 789);
        REQUIRE(dynamic_cast<EPubPlace *>(place1.get())->getHRef() == "chapter1.html");

        // Test EPUB with complex href
        auto place2 = Place::factory("[EPUB;offset:123,href:folder/chapter2.xhtml]");
        REQUIRE(place2->isFor(DocType::EPUB));
        REQUIRE(place2->getOffset() == 123);
        REQUIRE(dynamic_cast<EPubPlace *>(place2.get())->getHRef() == "folder/chapter2.xhtml");

        // Test invalid EPUB format returns empty place
        auto place3 = Place::factory("[EPUB;offset:invalid,href:chapter.html]");
        REQUIRE(place3->isFor(DocType::EPUB));
        REQUIRE(place3->getOffset() == 0);
        REQUIRE(dynamic_cast<EPubPlace *>(place3.get())->getHRef() == "chapter.html");

        // Test with spaces and nested brackets
        auto place4 = Place::factory("[EPUB;offset:3146,href:Brandon Sanderson - [Mistborn 01] - "
                                     "The Final Empire_split_000.htm]");
        REQUIRE(place4->isFor(DocType::EPUB));
        REQUIRE(place4->getOffset() == 3146);
        REQUIRE(dynamic_cast<EPubPlace *>(place4.get())->getHRef() ==
                "Brandon Sanderson - [Mistborn 01] - The Final Empire_split_000.htm");
    }

    SECTION("Test factory overloads") {
        // Test offset-only factory
        auto place1 = Place::factory(123);
        REQUIRE(place1->isFor(DocType::TXT));
        REQUIRE(place1->getOffset() == 123);

        // Test offset+href factory
        auto place2 = Place::factory(456, "chapter.html");
        REQUIRE(place2->isFor(DocType::EPUB));
        REQUIRE(place2->getOffset() == 456);
        REQUIRE(dynamic_cast<EPubPlace *>(place2.get())->getHRef() == "chapter.html");
    }

    SECTION("Unpaired opening bracket in href") {
        auto place = Place::factory("[EPUB;offset:100,href:Bad[File.epub]", DocType::EPUB);
        REQUIRE(place->getOffset() == 100);
        REQUIRE(dynamic_cast<EPubPlace *>(place.get())->getHRef() == "Bad[File.epub");
    }

    SECTION("Multiple unpaired opening brackets") {
        auto place = Place::factory("[EPUB;offset:100,href:Bad[[File.epub]", DocType::EPUB);
        REQUIRE(place->getOffset() == 100);
        REQUIRE(dynamic_cast<EPubPlace *>(place.get())->getHRef() == "Bad[[File.epub");
    }

    SECTION("Unpaired closing bracket in href") {
        auto place = Place::factory("[EPUB;offset:100,href:Bad]File.epub]", DocType::EPUB);
        REQUIRE(place->getOffset() == 100);
        REQUIRE(dynamic_cast<EPubPlace *>(place.get())->getHRef() == "Bad]File.epub");
    }

    SECTION("Missing final closing bracket") {
        // Should return default place when format is invalid
        auto place = Place::factory("[EPUB;offset:100,href:file.epub", DocType::EPUB);
        REQUIRE(place->getOffset() == 0);
        REQUIRE(dynamic_cast<EPubPlace *>(place.get())->getHRef() == "");
    }
}
