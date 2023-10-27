#include <catch_amalgamated.hpp>

#include "Misc/GUnzip.hpp"
#include "Misc/GUnzipStream.hpp"
#include "Misc/GZipStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "ViewTestCommon.hpp"

static bool compareFiles(SFile &f1, SFile &f2) {
    if (f1.size() != f2.size()) {
        return false;
    }

    f1.seek(0);
    f2.seek(0);

    int size = f1.size();
    for (int i = 0; i < size; i++) {
        if (f1.read() != f2.read()) {
            return false;
        }
    }

    f1.seek(0);
    f2.seek(0);

    return true;
}

TEST_CASE("Test GZip Stream compression") {
    PosixFileSystem fs("./test/CatchTests/TestData");
    fs.begin();

    std::string uncFileName = "/book/last-question.txt";
    std::string compFileName = "/book/last-question.txt.stream.gz";

    SECTION("test") {
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
                int bytesToRead = gzstream.available() > sizeof(outputBuffer)
                                      ? sizeof(outputBuffer)
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
    SECTION("cleanup") { fs.remove(compFileName); }
}

TEST_CASE("Test Gunzip decompress file") {
    PosixFileSystem fs("./test/CatchTests/TestData");

    fs.begin();

    SECTION("test") {
        GUnzip gunzip;
        REQUIRE(gunzip.decompressFile(fs, "/book/old-testament.txt.gz",
                                      "/book/old-testament.decompressed.txt"));
        SFile f1 = fs.open("/book/old-testament.decompressed.txt");
        SFile f2 = fs.open("/book/old-testament.txt");
        REQUIRE(compareFiles(f1, f2));
    }
    SECTION("cleanup") { fs.remove("/book/old-testament.decompressed.txt"); }
}

TEST_CASE("Test Gunzip functionality") {
    PosixFileSystem fs("./test/CatchTests/TestData");
    fs.begin();

    SECTION("test") {
        SFile compFile = fs.open("/book/old-testament.txt.gz");
        SFile outFile = fs.open("/book/old-testament.decompressed.txt", "w", true);

        GUnzip gunzip;
        gunzip.setCompressedSize(compFile.size());

        uint8_t inBuffer[128];
        uint8_t outBuffer[128];

        // test worst case scenario - 1 byte input and output buffers
        int testBufSize = 1;

        bool done = false;
        int inputBytesAvailable = 0;
        int inputIndex = 0;
        do {
            if (inputBytesAvailable == 0) {
                // read more
                int read = compFile.readBytes((char *)inBuffer, testBufSize);
                if (read == 0) {
                    done = true;
                    break;
                }

                inputBytesAvailable = read;
                inputIndex = 0;
            }

            // push some input data
            int inputBytesRead = 0;
            int outputBytesAvail = 0;
            int err = gunzip.decompress(&inBuffer[inputIndex], inputBytesAvailable, outBuffer,
                                        testBufSize, inputBytesRead, outputBytesAvail);
            REQUIRE(((err == MZ_OK) || (err == MZ_STREAM_END)));

            if (err == MZ_STREAM_END) {
                done = true;
            }

            // decrease input bytes
            if (inputBytesRead > 0) {
                inputBytesAvailable -= inputBytesRead;
                inputIndex += inputBytesRead;
            }

            // keep reading until all output bytes are read
            bool lastOutput = false;
            while (outputBytesAvail > 0) {
                outFile.write(outBuffer, outputBytesAvail);
                outFile.flush();

                if (lastOutput) {
                    break;
                }

                err = gunzip.decompress(nullptr, 0, outBuffer, testBufSize, inputBytesRead,
                                        outputBytesAvail);

                if (err == MZ_BUF_ERROR) {
                    // no more output to read
                    lastOutput = true;
                }
            }
        } while (!done);

        compFile.close();
        outFile.close();
        SFile uncFile1 = fs.open("/book/old-testament.decompressed.txt");
        SFile uncFile2 = fs.open("/book/old-testament.txt");
        REQUIRE(compareFiles(uncFile1, uncFile2));
    }
    SECTION("cleanup") { fs.remove("/book/old-testament.decompressed.txt"); }
}

TEST_CASE("Test Gunzip stream reading") {
    PosixFileSystem fs("./test/CatchTests/TestData");
    fs.begin();

    SFile f1 = fs.open("/book/old-testament.txt.gz");
    GUnzipStream s1(f1);
    RendererStream *stream = &s1;
    REQUIRE(stream->size() == 3061230);

    auto streamz = fs.newGzipRendererStream("/book/old-testament.txt.gz");
    REQUIRE(streamz->size() == 3061230);

    SFile ufile = fs.open("/book/old-testament.txt");

    uint8_t buffer1[100];
    uint8_t buffer2[100];
    uint8_t buffer3[100];
    int bufferLen = sizeof(buffer1) - 1;

    int res1, res2, res3;
    int total = 0;
    do {
        memset(buffer1, 0, 100);
        memset(buffer2, 0, 100);
        memset(buffer3, 0, 100);

        res1 = stream->readBytes(buffer1, bufferLen);
        res2 = streamz->readBytes(buffer2, bufferLen);
        res3 = ufile.readBytes((char *)buffer3, bufferLen);

        REQUIRE(res1 == res2);
        REQUIRE(res1 == res3);

        total += res1;
        REQUIRE(memcmp(buffer1, buffer2, res1) == 0);
        REQUIRE(memcmp(buffer1, buffer3, res1) == 0);
    } while (res1 == bufferLen);
}

TEST_CASE("Test GZip stream handling") {
    PosixFileSystem fs("./test/CatchTests/TestData");
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
    REQUIRE(strcmp((char *)buffer2, "earth. The earth was without form and void, and darkness was "
                                    "over the face of the deep. And the Spi") == 0);

    // Seek past the look back buffer threshold
    stream->seek(50000);
    REQUIRE(stream->position() == 50000);
    stream->readBytes(buffer, bufferLen);
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

    // seek all the way to the end
    stream->seek(3061220);
    // try to read one byte past the end
    int bytesRead = stream->readBytes(buffer, 11);
    // stream is at the end now
    REQUIRE(bytesRead == 10);
    REQUIRE(stream->position() == 3061230);
    // seek back to the start
    stream->seek(0);

    // re-test the beginning
    // Grab the very start of the file
    stream->readBytes(buffer, bufferLen);
    REQUIRE(stream->position() == bufferLen);
    // log_d("Got buffer: \n%s", buffer);
    REQUIRE(strcmp((char *)buffer, "Old Testament\n\nGenesis (50 Chapters)\n\n\nGENESIS 1 In the "
                                   "beginning, God created the heavens and the ") == 0);
}
