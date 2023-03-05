#include <catch_amalgamated.hpp>

#include "Storage/PosixFileSystem.hpp"
#include "ViewTestCommon.hpp"

TEST_CASE("Test GZip stream handling") {
    SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();

    auto stream = fs.newGzipRendererStream("/old-testament.txt.gz");
    REQUIRE(stream->size() == 3061230);

    uint8_t buffer[100];
    int bufferLen = sizeof(buffer) - 1;
    buffer[sizeof(buffer) - 1] = 0;

    // Grab the very start of the file
    stream->readBytes(buffer, bufferLen);
    REQUIRE(stream->position() == bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, "Old Testament\n\nGenesis (50 Chapters)\n\n\nGENESIS 1 In the "
                                   "beginning, God created the heavens and the ") == 0);

    // Grab the next buffer
    uint8_t buffer2[100];
    int bufferLen2 = sizeof(buffer2) - 1;
    buffer2[sizeof(buffer2) - 1] = 0;
    stream->readBytes(buffer2, bufferLen2);
    // log_d("Got buffer: \n%s", buffer2);
    REQUIRE(strcmp((char *)buffer2, "earth. The earth was without form and void, and darkness was "
                                    "over the face of the deep. And the Spi") == 0);

    // Seek past the look back buffer threshold
    stream->seek(50000);
    REQUIRE(stream->position() == 50000);
    stream->readBytes(buffer, bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, "e they ate.\n\nThey said to him, \"Where is Sarah your wife?\" "
                                   "And he said, \"She is in the tent.\" The L") == 0);

    // Seek back just a tad
    stream->seek(49990);
    REQUIRE(stream->position() == 49990);
    stream->readBytes(buffer, bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, " tree while they ate.\n\nThey said to him, \"Where is Sarah "
                                   "your wife?\" And he said, \"She is in the te") == 0);

    // Seek back to the second buffer and make sure it's the same
    stream->seek(bufferLen);
    REQUIRE(stream->position() == bufferLen);
    stream->readBytes(buffer, bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, (char *)buffer2) == 0);

    // Seek wayy forward
    stream->seek(1000000);
    REQUIRE(stream->position() == 1000000);
    stream->readBytes(buffer, bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, " displeased Samuel when they said, \"Give us a king to judge "
                                   "us.\" And Samuel prayed to the LORD. And") == 0);

    // Now seek a ways back
    stream->seek(500000);
    REQUIRE(stream->position() == 500000);
    stream->readBytes(buffer, bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, "f Judah. And his offering was one silver plate whose weight "
                                   "was 130 shekels, one silver basin of 70") == 0);
}
