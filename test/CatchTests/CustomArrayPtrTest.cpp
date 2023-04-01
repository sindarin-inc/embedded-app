#include <catch_amalgamated.hpp>

#include "Misc/SpiramAllocator.hpp"
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
