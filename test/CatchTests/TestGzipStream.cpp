#include <catch_amalgamated.hpp>

#include "Misc/GUnzip.hpp"
#include "Misc/GZip.hpp"
#include "Misc/GZipStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "ViewTestCommon.hpp"

TEST_CASE("Test GZip compression") {
    SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();

    auto file = fs.open("/book/last-question.txt");
    auto outFile = fs.open("/book/last-question.txt.gz", "w", true);

    GZip gzip;
    bool ret = gzip.initWithOutputFile(outFile);
    REQUIRE(ret == true);

    ret = gzip.compressFile(file);
    REQUIRE(ret == true);

    file.close();

    auto stream = fs.newGzipRendererStream("/book/last-question.txt.gz");
    REQUIRE(stream->size() == 25924);

    uint8_t testBuffer[2][25924];
    stream->readBytes(testBuffer[0], 25924);
    REQUIRE(stream->position() == 25924);

    file = fs.open("/book/last-question.txt");
    REQUIRE(file.readBytes((char *)testBuffer[1], 25924) == 25924);

    REQUIRE(memcmp(testBuffer[0], testBuffer[1], 25924) == 0);
}

TEST_CASE("Test GZip Stream compression") {
    SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();

    std::string uncFileName = "/book/last-question.txt";
    std::string compFileName = "/book/last-question.txt.stream.gz";

    auto file = fs.open(uncFileName);
    auto outFile = fs.open(compFileName, "w", true);

    GZipStream gzstream;
    uint8_t inputBuffer[1024];
    uint8_t outputBuffer[1024];
    int fileSize = file.size();
    int uncompressedSize = fileSize;

    REQUIRE(uncompressedSize == 25924);
    REQUIRE(gzstream.init());
    do {
        // read chunk of file
        int bytesToRead =
            file.available() < sizeof(inputBuffer) ? file.available() : sizeof(inputBuffer);
        size_t r = file.read(inputBuffer, bytesToRead);
        REQUIRE(r == bytesToRead);

        // write chunk to gzip stream
        REQUIRE(gzstream.write(inputBuffer, bytesToRead));

        while (gzstream.available() > 0) {
            int bytesToRead = gzstream.available() > sizeof(outputBuffer) ? sizeof(outputBuffer)
                                                                          : gzstream.available();
            REQUIRE(gzstream.read(outputBuffer, bytesToRead) == bytesToRead);
            if (bytesToRead > 0) {
                outFile.write(outputBuffer, bytesToRead);
            }
        }
    } while (file.available() > 0);

    REQUIRE(gzstream.flush());
    while (gzstream.available() > 0) {
        int bytesToRead = gzstream.available() > sizeof(outputBuffer) ? sizeof(outputBuffer)
                                                                      : gzstream.available();
        REQUIRE(gzstream.read(outputBuffer, bytesToRead) == bytesToRead);
        if (bytesToRead > 0) {
            outFile.write(outputBuffer, bytesToRead);
        }
    }

    file.close();
    outFile.close();

    outFile = fs.open(compFileName, "r");
    outFile.close();

    auto stream = fs.newGzipRendererStream(compFileName.c_str());
    REQUIRE(stream->size() == uncompressedSize);

    uint8_t *testBuffer[2];
    for (int i = 0; i < 2; i++) {
        testBuffer[i] = (uint8_t *)malloc(uncompressedSize);
    }

    stream->readBytes(testBuffer[0], uncompressedSize);
    REQUIRE(stream->position() == uncompressedSize);

    file = fs.open(uncFileName);
    REQUIRE(file.readBytes((char *)testBuffer[1], uncompressedSize) == uncompressedSize);

    REQUIRE(memcmp(testBuffer[0], testBuffer[1], uncompressedSize) == 0);

    for (int i = 0; i < 2; i++) {
        free(testBuffer[i]);
        testBuffer[i] = NULL;
    }
}

TEST_CASE("Test GZip stream handling") {
    SetupFileSystemMock("TestData");

    PosixFileSystem fs;
    fs.begin();

    auto stream = fs.newGzipRendererStream("/book/old-testament.txt.gz");
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
