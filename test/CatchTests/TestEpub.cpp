#include <catch_amalgamated.hpp>

#include "Communications/HtmlCleaner.hpp"
#include "EPub/EPubFile.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "ViewTestCommon.hpp"

TEST_CASE("Test ePub parsing") {
    SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();

    EPubFile epub(fs, "/butcher.epub");
    REQUIRE(epub.open());

    uint8_t buffer[1025];
    int bufferLen = sizeof(buffer) - 1;
    buffer[sizeof(buffer) - 1] = 0;
    epub.readBytes(buffer, sizeof(buffer) - 1);
    REQUIRE(epub.position() == bufferLen);
    REQUIRE(epub.size() == 76147);
    REQUIRE(epub.available() == 76147 - bufferLen);

    epub.seek(9000);
    epub.seek(0);
    uint8_t buffer2[1025];
    buffer2[sizeof(buffer2) - 1] = 0;
    epub.readBytes(buffer2, sizeof(buffer2) - 1);
    REQUIRE(epub.available() == 76147 - bufferLen);
    REQUIRE(strncmp((char *)buffer, (char *)buffer2, sizeof(buffer)) == 0);

    epub.close();
}

TEST_CASE("Stripped ePub") {
    SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();

    auto epubFile = std::make_unique<EPubFile>(fs, "/butcher.epub");
    epubFile->open();
    auto stream = std::make_shared<HtmlCleaner>(std::move(epubFile));

    uint8_t buffer[1025];
    int bufferLen = sizeof(buffer) - 1;
    // Null terminate for easy logging
    buffer[sizeof(buffer) - 1] = 0;
    stream->readBytes(buffer, bufferLen);
    REQUIRE(stream->position() == bufferLen);
    // log_d("Got buffer: \n%s", buffer);

    stream->seek(0);
    uint8_t buffer2[1025];
    buffer2[sizeof(buffer2) - 1] = 0;
    stream->readBytes(buffer2, bufferLen);
    // log_d("Got buffer2: \n%s", buffer2);

    REQUIRE(strncmp((char *)buffer, (char *)buffer2, bufferLen) == 0);

    // Seek forward
    stream->seek(3000);
    stream->readBytes(buffer, bufferLen / 2);
    stream->readBytes(buffer + bufferLen / 2, bufferLen / 2);
    REQUIRE(stream->position() == 3000 + bufferLen);
    // log_d("Got buffer: \n%s", buffer);

    stream->seek(4000);
    stream->seek(3000);
    stream->readBytes(buffer2, bufferLen / 2);
    stream->readBytes(buffer2 + bufferLen / 2, bufferLen / 2);
    // log_d("Got buffer2: \n%s", buffer2);

    REQUIRE(strncmp((char *)buffer, (char *)buffer2, bufferLen / 2) == 0);
    REQUIRE(strncmp((char *)buffer + bufferLen / 2, (char *)buffer2 + bufferLen / 2,
                    bufferLen / 2) == 0);
}