#include <Spiram/Memory.hpp>
#include <Spiram/String.hpp>
#include <StringUtil/StringUtil.hpp>
#include <catch_amalgamated.hpp>

#include "Renderers/RendererStream.hpp"
#include "sindarin-debug.h"

int mallocSize;
void *mallocPtr;
void *freedPtr;

void *mymalloc(size_t size) {
    mallocSize = size;
    mallocPtr = (void *)malloc(size);
    log_d("Mallocd %zu bytes in %p", size, mallocPtr);
    return mallocPtr;
}

void myfree(void *p) {
    freedPtr = p;
    log_d("Freeing %p", p);
    free(p);
}

TEST_CASE("Test CustomArrayPtr") {
    void *ptrToFree;
    {
        constexpr size_t BUFFER_SIZE = 2097152;
        std::shared_ptr<uint8_t[]> buffer;

        // Try creating in a lambda to imitate how it's used in FileDownload
        [&buffer]() { buffer = CustomArrayPtr<uint8_t>(BUFFER_SIZE, &mymalloc, &myfree); }();

        ptrToFree = buffer.get();
        REQUIRE(mallocSize == BUFFER_SIZE);
        REQUIRE(mallocPtr == buffer.get());

        // Fill the buffer with random data
        std::srand(std::time(nullptr));
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            buffer[i] = i % 256;
        }

        bool equal = true;
        // Read back the data and check that it is correct
        for (size_t i = 0; i < BUFFER_SIZE; ++i) {
            if (buffer[i] != (i % 256)) {
                log_e("buffer[%zu] != %d", i, buffer[i]);
                equal = false;
                break;
            }
        }
        REQUIRE(equal);
    }

    REQUIRE(freedPtr == ptrToFree);
}

TEST_CASE("Test SpiramIStream") {
    SpiramString message =
        SpiRamStringFormat("New firmware %s is now available! Here's what's new:\n\n", "0.2.0");
    auto strinstream = std::make_shared<SpiramIStream>(message);
    REQUIRE(strinstream->get() == 'N');
    REQUIRE(strinstream->get() == 'e');
    // REQUIRE(strinstream->tellg() == 0);
    REQUIRE(strinstream->good());
    REQUIRE(!strinstream->eof());
    strinstream->seekg(0, std::ios::beg);
    REQUIRE(strinstream->get() == 'N');
    REQUIRE(strinstream->get() == 'e');
    REQUIRE(strinstream->get() == 'w');
    REQUIRE(strinstream->get() == ' ');
    strinstream->seekg(0, std::ios::beg);

    // Now make a StdStringStream
    auto stream = std::make_shared<StdStringStream>(message);
    REQUIRE(stream->available() == 57);
    REQUIRE(stream->read() == 'N');
    REQUIRE(stream->read() == 'e');
    REQUIRE(stream->available() == 55);
}
